#!/usr/bin/env python3

# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

import os
import sys
import re
import csv
import string
import argparse

from collections import defaultdict

# Define global dictionaries and sets
mDictionaryOfSuspectWords = defaultdict(list)

# Defining some words here as blackduck CI scan doesn't allow them in the txt file
mWordWhitelist = {
    "c" + "opyright",
    "c" + "opyrightable",
    "c" + "opyrighted",
    "c" + "opyrighter",
    "c" + "opyrighting",
    "c" + "opyrights",
    "re" + "co " + "py" + "right",
    "un" + "copy" + "righted"
}
mFileWhitelist = set()

# Convert a word to lowercase
def to_lower(word):
    return word.lower()

# Load dictionary from a file
def load_dictionary_helper(path):
    try:
        with open(path, "r", encoding="utf-8") as file:
            for word in file:
                word = word.strip()
                if word:
                    word = to_lower(word)
                    if "." in word:
                        mFileWhitelist.add(word)
                    else:
                        mWordWhitelist.add(word)
        
    except FileNotFoundError:
        print(f"Dictionary file not found: {path}")

# Load whitelisted words
def load_dictionary():
    load_dictionary_helper("extra.txt")
    load_dictionary_helper("words_alpha.txt")

# Process a word and check against the whitelist
def process_word(word, path, lineno):
    part = ""
    i = 0
    while i < len(word):
        c = word[i]
        if part:
            # Detect CamelCase or non-alpha separators
            if not c.isalpha() or (part[-1].islower() and c.isupper()):
                part = to_lower(part)

                # Remove known prefixes
                prefixes = ["current", "custom", "content", "hdcp", "hdmi", "drm", "aamp",
                            "abr", "http", "json", "js", "video", "uri", "url", "tune",
                            "tsb", "cmcd", "curl", "mpd", "lld"]
                for prefix in prefixes:
                    if part.startswith(prefix) and len(part) > len(prefix):
                        part = part[len(prefix):]
                        break

                # Add to suspect words if not in whitelist and longer than 4 characters
                if part and part not in mWordWhitelist and len(part) > 4:
                    mDictionaryOfSuspectWords[part].append((path, lineno, word))

                part = ""

        # Build next token
        if c.isalpha():
            part += c

        # Skip hex literals (0x123ABC)
        if c == '0' and i + 1 < len(word) and word[i + 1] == 'x':
            i += 2
            while i < len(word) and i < len(word) and word[i].isalnum():
                i += 1
            continue  # Skip the rest of the loop for this index

        i += 1

# Process a single line
def process_line(line, path, lineno):
    # Skip certain lines with markers
    markers = ["<scte35:Binary>", "// psshData", "// base64", "<cenc:pssh>",
               "_UUID ", "AAMP_CFG_BASE64=", ",IV=", "PTS/DTS format:",
               "slice start:", "subtec-app checkout"]
    
    if any(marker in line for marker in markers):
        return
    
    process_word(line, path, lineno)

# Process a file
def process_file(file_path):
    try:
        with open(file_path, "r", encoding="utf-8") as file:
            for lineno, line in enumerate(file, start=1):
                process_line(line, file_path, lineno)
    except Exception as e:
        print(f"Error reading file {file_path}: {e}")

# Scan the directory and process files
def scan(dir_path):
    if not os.path.isdir(dir_path):
        print(f"Error: Invalid directory path: {dir_path}")
        return
    
    directories_to_skip = {".libs", "build", "l2env", "lib", "test"}
    extensions_to_process = {".cpp", ".c", ".hpp", ".h", ".py", ".sh", ".txt", ".md", ".html", ".js"}

    for root, dirs, files in os.walk(dir_path):
        dirs[:] = [d for d in dirs if d not in directories_to_skip]  # Skip certain directories

        for file in files:
            if file in mFileWhitelist:
                continue
            
            if any(file.endswith(ext) for ext in extensions_to_process):
                process_file(os.path.join(root, file))

# Dump results to the CSV file
def dump_dictionary():
    output_file = "typos.csv"
    with open(output_file, "w", newline="", encoding="utf-8") as fout:
        writer = csv.writer(fout)
        writer.writerow(["Word", "Occurrences", "Locations", "Original Text"])
        
        for word, occurrences in mDictionaryOfSuspectWords.items():
            locations = "\n".join(f"{path}:{lineno}" for path, lineno, _ in occurrences)
            original_texts = "\n".join(orig_text.replace('"', '`') for _, _, orig_text in occurrences)
            writer.writerow([word, len(occurrences), locations, original_texts])

# Main function
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Find and report suspect words in source files.")
    parser.add_argument("directory", nargs="?", default="../../../", help="Directory to scan for source files. Default '../../../'.")
    args = parser.parse_args()


    if args.directory:
        dir_path = args.directory
    else:
        dir_path = "../../../"

    if not os.path.isdir(dir_path):
        print(f"Error: Invalid directory path: {dir_path}")
    
    load_dictionary()
    scan(dir_path)
    dump_dictionary()
