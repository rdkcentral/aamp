/*
 * If not stated otherwise in this file or this component's LICENSE file the
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

#ifndef ISOBMFFTESTDATA
#define ISOBMFFTESTDATA

/* Data for individual boxes */
static const uint8_t sencSingleSample[] = { 0x00, 0x00, 0x00, 0x20, 0x73, 0x65, 0x6e, 0x63, 0x00, 0x00, 0x00, 0x02,
									0x00, 0x00, 0x00, 0x01, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
									0x00, 0x01, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30 };

static const uint8_t sencSingleSample_expected[] = { 0x00, 0x00, 0x00, 0x20, 0x73, 0x65, 0x6e, 0x63, 0x00, 0x00, 0x00, 0x02,
									0x00, 0x00, 0x00, 0x01, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
									0x00, 0x01, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30 };

static const uint8_t saizSingleSample[] = { 0x00, 0x00, 0x00, 0x1a, 0x73, 0x61, 0x69, 0x7a, 0x00, 0x00, 0x00, 0x01,
									0x63, 0x65, 0x6e, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
									0x01, 0x10 };

static const uint8_t saizSingleSample_expected[] = { 0x00, 0x00, 0x00, 0x1a, 0x73, 0x61, 0x69, 0x7a, 0x00, 0x00, 0x00, 0x01,
									0x63, 0x65, 0x6e, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
									0x01, 0x10 };

static const uint8_t trunSingleEntryTrackDefaults[] = {
    0x00, 0x00, 0x00, 0x18,  't',  'r',  'u',  'n', 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00};

static const uint8_t trunSingleEntryTrackDefaults_expected[] = {
    0x00, 0x00, 0x00, 0x18,  't',  'r',  'u',  'n', 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00};

static const uint8_t mdatData[] = { 0x00, 0x00, 0x04, 0x00, 'm', 'd', 'a', 't' };

static const uint8_t tfhdDefaultSampleDurationPresent[]{
	0x00, 0x00, 0x00, 0x1C,  't',  'f',  'h',  'd',
	0x00, 0x02, 0x00, 0x38, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xAB, 0xCA,
	0x01, 0x01, 0x00, 0x00 };

static const uint8_t tfhdDefaultSampleDurationAbsent[]{
	0x00, 0x00, 0x00, 0x1C,  't',  'f',  'h',  'd',
	0x00, 0x02, 0x00, 0x30, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xAB, 0xCA,
	0x01, 0x01, 0x00, 0x00 };

// 32-bit base media decode time
static const uint8_t tfdtDataV0[]{
	0x00, 0x00, 0x00, 0x10,  't',  'f',  'd',  't',
	0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x24, 0x00 };

// 64-bit base media decode time
static const uint8_t tfdtDataV1[]{
	0x00, 0x00, 0x00, 0x14,  't',  'f',  'd',  't',
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x13, 0x24, 0x00 };

//
//
//


static const uint8_t sencMultipleSample[] = {
    0x00, 0x00, 0x00, 0x40,  's',  'e',  'n',  'c', 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x01, 0x00, 0x01, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x02, 0x00, 0x01, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30};

static const uint8_t sencMultipleSample_expected[] = {
    0x00, 0x00, 0x00, 0x20,  's',  'e',  'n',  'c', 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x00, 0x01, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30,
    0x00, 0x00, 0x00, 0x20,  's',  'k',  'i',  'p', 0x00, 0x01, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x02, 0x00, 0x01, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30};

static const uint8_t sencMultipleSampleMultipleVaryingSubSamples[] = {
    0x00, 0x00, 0x00, 0x52,  's',  'e',  'n',  'c', 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x00, 0x02, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30,
    0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x01, 0x00, 0x03,
    0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30, 0x00, 0xb5, 0x00, 0x02,
    0x5a, 0x30, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x02, 0x00, 0x01, 0x00, 0xb5, 0x00, 0x02,
    0x5a, 0x30};

static const uint8_t sencMultipleSampleMultipleVaryingSubSamples_expected[] = {
    0x00, 0x00, 0x00, 0x26,  's',  'e',  'n',  'c', 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x00, 0x02, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30,
    0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30, 0x00, 0x00, 0x00, 0x2c,  's',  'k',  'i',  'p', 0x00, 0x03,
    0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30, 0x00, 0xb5, 0x00, 0x02, 0x5a, 0x30, 0x00, 0xb5, 0x00, 0x02,
    0x5a, 0x30, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x02, 0x00, 0x01, 0x00, 0xb5, 0x00, 0x02,
    0x5a, 0x30};

static const uint8_t saizMultipleSampleNoSkip[] = {
    0x00, 0x00, 0x00, 0x1c,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x01, 0x63, 0x65, 0x6e, 0x63,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03};

static const uint8_t saizMultipleSampleNoSkip_expected[] = {
    0x00, 0x00, 0x00, 0x1c,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x01, 0x63, 0x65, 0x6e, 0x63,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x03};

static const uint8_t saizMultipleSample[] = {
    0x00, 0x00, 0x00, 0x30,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x01, 0x63, 0x65, 0x6e, 0x63,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};

static const uint8_t saizMultipleSample_expected[] = {
    0x00, 0x00, 0x00, 0x1a,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x01, 0x63, 0x65, 0x6e, 0x63,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x16,  's',  'k',
     'i',  'p', 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};

static const uint8_t saizDefaultInfo[] = {
    0x00, 0x00, 0x00, 0x19,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x01, 0x63, 0x65, 0x6e, 0x63,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x05};

static const uint8_t saizDefaultInfo_expected[] = {
    0x00, 0x00, 0x00, 0x19,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x01, 0x63, 0x65, 0x6e, 0x63,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01};

static const uint8_t saizMultipleSampleNoSkipNoFlag[] = {
    0x00, 0x00, 0x00, 0x14,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0x01, 0x02, 0x03};

static const uint8_t saizMultipleSampleNoSkipNoFlag_expected[] = {
    0x00, 0x00, 0x00, 0x14,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x02, 0x03};

static const uint8_t saizMultipleSampleNoFlag[] = {
    0x00, 0x00, 0x00, 0x28,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x13, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};

static const uint8_t saizMultipleSampleNoFlag_expected[] = {
    0x00, 0x00, 0x00, 0x12,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x16,  's',  'k',  'i',  'p', 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};

static const uint8_t saizDefaultInfoNoFlag[] = {
    0x00, 0x00, 0x00, 0x19,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x05};

static const uint8_t saizDefaultInfoNoFlag_expected[] = {
    0x00, 0x00, 0x00, 0x19,  's',  'a',  'i',  'z', 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x01};



static const uint8_t trunMultipleEntryTrackDefaults[] = {
    0x00, 0x00, 0x00, 0x18,  't',  'r',  'u',  'n', 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00};

static const uint8_t trunMultipleEntryTrackDefaults_expected[] = {
    0x00, 0x00, 0x00, 0x18,  't',  'r',  'u',  'n', 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00};

static const uint8_t trunMultipleEntryDurationNoSkip[] = {
    0x00, 0x00, 0x00, 0x20,  't',  'r',  'u',  'n', 0x00, 0x00, 0x01, 0x05, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd1, 0x00, 0x00, 0x00, 0xd2};

static const uint8_t trunMultipleEntryDurationNoSkip_expected[] = {
    0x00, 0x00, 0x00, 0x20,  't',  'r',  'u',  'n', 0x00, 0x00, 0x01, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xa3, 0x00, 0x00, 0x00, 0xd2};

static const uint8_t trunMultipleEntryDuration[] = {
    0x00, 0x00, 0x00, 0x24,  't',  'r',  'u',  'n', 0x00, 0x00, 0x01, 0x05, 0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd1, 0x00, 0x00, 0x00, 0xd2,
    0x00, 0x00, 0x00, 0xd3};

static const uint8_t trunMultipleEntryDuration_expected[] = {
    0x00, 0x00, 0x00, 0x1c,  't',  'r',  'u',  'n', 0x00, 0x00, 0x01, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x76, 0x00, 0x00, 0x00, 0x08,
     's',  'k',  'i',  'p'};

static const uint8_t trunMultipleEntrySize[] = {
    0x00, 0x00, 0x00, 0x24,  't',  'r',  'u',  'n', 0x00, 0x00, 0x02, 0x05, 0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x03};

static const uint8_t trunMultipleEntrySize_expected[] = {
    0x00, 0x00, 0x00, 0x24,  't',  'r',  'u',  'n', 0x00, 0x00, 0x02, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x08,
     's',  'k',  'i',  'p'};

static const uint8_t trunMultipleEntrySizeDuration[] = {
    0x00, 0x00, 0x00, 0x30,  't',  'r',  'u',  'n', 0x00, 0x00, 0x03, 0x05, 0x00, 0x00, 0x00, 0x03,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd1, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0xd2, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0xd3, 0x00, 0x00, 0x00, 0x03};

static const uint8_t trunMultipleEntrySizeDuration_expected[] = {
    0x00, 0x00, 0x00, 0x30,  't',  'r',  'u',  'n', 0x00, 0x00, 0x03, 0x05, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x6a, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x76, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x10,  's',  'k',  'i',  'p', 0x00, 0x00, 0x00, 0xd3, 0x00, 0x00, 0x00, 0x03};

static const uint8_t exampleMdatBox[] = {
	0x00, 0x00, 0x00, 0x0c,  'm',  'd',  'a',  't', 0x00, 0x00, 0x00, 0x00};

#endif
