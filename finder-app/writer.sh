#!/bin/bash

writefile="$1"
writestr="$2"

if [ -z "$writefile" ] || [ -z "$writestr" ]; then
    echo "Usage: $0 <writefile> <writestr>"
    exit 1
fi


mkdir -p "$(dirname "$writefile")"

if echo "$writestr" > "$writefile"; then
    echo "File created successfully: $writefile"
else
    echo "Error: Could not create file $writefile"
    exit 1
fi
