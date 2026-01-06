#!/bin/bash

EXCLUDE_DIR="./src/external"

total_lines=0

while IFS= read -r file; do
    if [ -f "$file" ]; then
        lines_in_file=$(wc -l < "$file" 2>/dev/null)
        if [ $? -eq 0 ] && [ -n "$lines_in_file" ]; then
            total_lines=$((total_lines + lines_in_file))
        fi
    fi
done < <(find . -type f \( -name "*.c" -o -name "*.h" \) -not -path "$EXCLUDE_DIR/*")

echo $total_lines
