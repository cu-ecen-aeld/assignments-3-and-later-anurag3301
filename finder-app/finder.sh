#!/bin/sh

filesdir="$1"
searchstr="$2"

if [ -z "$filesdir" ] || [ -z "$searchstr" ]; then
    echo "Usage: $0 <filesdir> <searchstr>"
    exit 1
fi

if [ ! -d "$filesdir" ]; then
    echo "$filesdir not found"
    exit 1
fi

file_count=$(find "$filesdir" -type f | wc -l)
match_count=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $file_count and the number of matching lines are $match_count"
