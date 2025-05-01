#!/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import re
import sys
import os
import hashlib
from pathlib import Path
import argparse

def time_to_seconds(t):
    parts = t.strip().split(":")
    seconds, millis = map(int, parts[1].split("."))
    return int(parts[0]) * 60 + seconds + millis / 1000

def seconds_to_time(t):
    minutes = int(t // 60)
    seconds = int(t % 60)
    millis = int(round((t - int(t)) * 1000))
    return f"{minutes:02}:{seconds:02}.{millis:03}"

def parse_vtt(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    blocks = []
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if re.match(r'\d{2}:\d{2}\.\d{3} --> \d{2}:\d{2}\.\d{3}', line):
            start, end = line.split(" --> ")
            start_sec = time_to_seconds(start)
            end_sec = time_to_seconds(end)

            content = []
            i += 1
            while i < len(lines) and lines[i].strip() != "":
                content.append(lines[i])
                i += 1

            blocks.append({
                "start": start_sec,
                "end": end_sec,
                "timing": line,
                "content": content
            })
        i += 1
    return blocks

def write_chunks(blocks, output_dir, chunk_duration):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    max_end = max(block['end'] for block in blocks)
    num_chunks = int(max_end // chunk_duration) + 1

    for i in range(num_chunks):
        chunk_start = i * chunk_duration
        chunk_end = chunk_start + chunk_duration

        chunk_blocks = [
            block for block in blocks
            if not (block['end'] <= chunk_start or block['start'] >= chunk_end)
        ]

        if chunk_blocks:
            filename = os.path.join(output_dir, f"chunk-{i}.vtt")
            with open(filename, 'w', encoding='utf-8') as out:
                out.write("WEBVTT\n\n")
                for block in chunk_blocks:
                    out.write(f"{block['timing']}\n")
                    out.writelines(block['content'])
                    out.write("\n")
    print(f"Chunks written to: {output_dir}/")

def block_hash(block):
    """Generate a hash key for a subtitle block based on timing and content."""
    content_str = ''.join(block['content']).strip()
    return hashlib.md5(f"{block['start']}-{block['end']}-{content_str}".encode()).hexdigest()

def combine_vtt_files(input_dir, output_file):
    seen_hashes = set()
    all_blocks = []

    for filename in sorted(os.listdir(input_dir)):
        if filename.endswith(".vtt"):
            path = os.path.join(input_dir, filename)
            blocks = parse_vtt(path)
            for block in blocks:
                bh = block_hash(block)
                if bh not in seen_hashes:
                    seen_hashes.add(bh)
                    all_blocks.append(block)

    all_blocks.sort(key=lambda b: b["start"])

    with open(output_file, "w", encoding="utf-8") as out:
        out.write("WEBVTT\n\n")
        for block in all_blocks:
            out.write(f"{block['timing']}\n")
            out.writelines(block['content'])
            out.write("\n")

    print(f"Combined VTT saved to: {output_file}")

def main():
    parser = argparse.ArgumentParser(description="Split or combine VTT files.")
    parser.add_argument("input_path", type=str, help="Input .vtt file or directory of .vtt files")
    parser.add_argument("--splitPeriod", type=int, default=10, help="Split period in seconds (for splitting only)")
    parser.add_argument("--output", type=str, help="Output file or directory (optional)")
    args = parser.parse_args()

    input_path = args.input_path
    split_period = args.splitPeriod
    output_path = args.output

    if os.path.isdir(input_path):
        # Combine
        if not output_path:
            output_path = f"{Path(input_path).name}_combined.vtt"
        combine_vtt_files(input_path, output_path)
    elif input_path.endswith(".vtt"):
        # Split
        if not os.path.exists(input_path):
            print(f"Error: {input_path} does not exist")
            return
        base = Path(input_path).stem
        output_dir = output_path if output_path else f"{base}_chunks"
        blocks = parse_vtt(input_path)
        write_chunks(blocks, output_dir, split_period)
    else:
        print("Error: Input must be a .vtt file or a directory containing .vtt files")

if __name__ == "__main__":
    main()