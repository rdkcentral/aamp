#!/bin/bash

# URL of the file to download
FILE_URL=$1

# Download the file and unzip
if [[ ! -f "$(basename "$FILE_URL")" ]]; then
    curl -O "$FILE_URL"
else
    echo "File already exists. Skipping download."
fi

FILE_NAME=$(basename "$FILE_URL")

if [[ -f "$FILE_NAME" ]]; then
    echo "Download complete. Extracting $FILE_NAME..."
    tar -xvf "$FILE_NAME"
else
    echo "Failed to unzip $FILE_NAME."
fi

# Replace the placeholder in $3 with the host IP and port
if [[ -n "$2" && -n "$3" && -n "$4" ]]; then
    SIMLINEAR_BASE_URL="http://$2:$3"
    sed -i.bak "s|var simlinearBaseUrl = .*;|var simlinearBaseUrl = \"$SIMLINEAR_BASE_URL\";|" $4
else
    echo "Error: Host IP, port, and file path must be provided as arguments."
    exit 1
fi