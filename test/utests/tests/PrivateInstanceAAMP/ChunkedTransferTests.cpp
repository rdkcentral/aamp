/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstddef>

#include "AampCurlStore.h"
#include "priv_aamp.h"
#include "AampUtils.h"  // hexCharToInt

extern void AampGrowableBuffer_EnableMemoryCopying(bool enable);

// Minimal helper to drive the callback with a given input,
// preserving state across invocations.
struct ChunkHarness {
	PrivateInstanceAAMP aamp;
	AampGrowableBuffer buffer;
	CurlCallbackContext ctx;

	ChunkHarness() {
		AampGrowableBuffer_EnableMemoryCopying(true);
		
		// Initialize context as parser expects
		ctx.m_ChunkedTransferState = ChunkedTransferState::READING_CHUNK_SIZE;
		ctx.m_ChunkedBytesRemaining = 0;
		ctx.buffer = &buffer;
		ctx.aamp = &aamp;
		ctx.mediaType = eMEDIATYPE_VIDEO;
	}

	void feed(const std::string& bytes) {
		aamp.chunked_write_callback(bytes.data(), bytes.size(), &ctx);
	}
	
	std::string GetBufferAsString()
	{
		return std::string( buffer.GetPtr(), buffer.GetLen() );
	}
};

// ---- Tests ----

TEST(ChunkedWriteCallback, SingleChunk_ExactBuffer) {
	ChunkHarness h;
	// "A\r\n" size line; "abcdefghij" payload; "\r\n" chunk end
	h.feed("A\r\nabcdefghij\r\n");

	EXPECT_EQ(h.GetBufferAsString(), "abcdefghij");
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::READING_CHUNK_SIZE);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 0u);
}

TEST(ChunkedWriteCallback, SplitAcrossCallbacks_SizeLineBoundary) {
	ChunkHarness h;

	// First callback: only "A\r"
	h.feed("A\r");
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::PENDING_CHUNK_START_LF);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 10u);

	// Second callback: LF + data + CRLF
	h.feed("\nabcdefghij\r\n");
	EXPECT_EQ(h.GetBufferAsString(), "abcdefghij");
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::READING_CHUNK_SIZE);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 0u);
}

TEST(ChunkedWriteCallback, SplitAcrossCallbacks_DataBoundary) {
	ChunkHarness h;

	h.feed("A\r\nabc");  // 3 bytes of the 10
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::READING_CHUNK_DATA);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 7u);
	EXPECT_EQ(h.GetBufferAsString(), "abc");

	h.feed("defghij\r\n"); // remaining 7 + CRLF
	EXPECT_EQ(h.GetBufferAsString(), "abcdefghij");
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::READING_CHUNK_SIZE);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 0u);
}

TEST(ChunkedWriteCallback, MultipleChunksInOneBuffer_ClampsExtra) {
	ChunkHarness h;

	// Two chunks concatenated: 3 and 2 bytes
	std::string payload = "3\r\nabc\r\n2\r\nde\r\n";
	h.feed(payload);

	EXPECT_EQ(h.GetBufferAsString(), "abcde");
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::READING_CHUNK_SIZE);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 0u);
}

TEST(ChunkedWriteCallback, ChunkExtensions_SameCallback) {
	ChunkHarness h;

	h.feed("A;foo=bar\r\nabcdefghij\r\n");
	EXPECT_EQ(h.GetBufferAsString(), "abcdefghij");
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::READING_CHUNK_SIZE);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 0u);
}

TEST(ChunkedWriteCallback, ChunkExtensions_SplitAcrossCallbacks) {
	ChunkHarness h;

	h.feed("A;foo");
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::READING_EXTENSIONS);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 10u);  // accumulation of 'A' is done

	h.feed("=bar\r\nabcdefghij\r\n");
	EXPECT_EQ(h.GetBufferAsString(), "abcdefghij");
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::READING_CHUNK_SIZE);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 0u);
}

TEST(ChunkedWriteCallback, TerminalZeroLengthChunk) {
	ChunkHarness h;

	// Terminal chunk: "0\r\n" then end marker CRLF
	h.feed("0\r\n\r\n");

	EXPECT_TRUE(h.GetBufferAsString().empty());
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::DONE);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 0u);
}

TEST(ChunkedWriteCallback, Error_InvalidHexCharInSize) {
	ChunkHarness h;

	h.feed("G\r\n"); // 'G' is invalid hex
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::ERROR);
	// Ensure no payload appended
	EXPECT_TRUE(h.GetBufferAsString().empty());
}

TEST(ChunkedWriteCallback, Error_MissingLF_AfterSizeCR) {
	ChunkHarness h;

	// Expect immediate error when the next char after CR is not '\n'
	h.feed("A\rX"); // 'X' not LF
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::ERROR);
	EXPECT_TRUE(h.GetBufferAsString().empty());
}

TEST(ChunkedWriteCallback, Error_MissingCR_AfterPayload) {
	ChunkHarness h;

	h.feed("3\r\nabcX"); // After consuming 3 bytes payload, next must be '\r'
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::ERROR);
}

TEST(ChunkedWriteCallback, Error_MissingLF_AfterCR) {
	ChunkHarness h;

	h.feed("1\r\na");           // payload complete
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::PENDING_CHUNK_END_CR);

	h.feed("X");                // not '\r' => ERROR
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::ERROR);
}

TEST(ChunkedWriteCallback, Error_BytesRemainingNotZeroOnChunkEndLF) {
	ChunkHarness h;

	// Make parser think there are bytes remaining (contrived)
	h.ctx.m_ChunkedTransferState = ChunkedTransferState::PENDING_CHUNK_END_LF;
	h.ctx.m_ChunkedBytesRemaining = 1;

	h.feed("\n"); // parser checks remaining != 0 => ERROR
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::ERROR);
}

TEST(ChunkedWriteCallback, ClampWhenBufferHasMoreThanNeeded) {
	ChunkHarness h;

	// Supply chunk size 2, but then give 2 bytes payload plus immediate CRLF and next chunk size char
	h.feed("2\r\nab\r\n3"); // the '3' at end should not be consumed as data
	EXPECT_EQ(h.GetBufferAsString(), "ab");
	EXPECT_EQ(h.ctx.m_ChunkedTransferState, ChunkedTransferState::READING_CHUNK_SIZE);
	EXPECT_EQ(h.ctx.m_ChunkedBytesRemaining, 3u);
}
