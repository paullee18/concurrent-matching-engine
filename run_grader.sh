#!/bin/bash

cd scripts

for file in *; do
    if [ -f "$file" ]; then
        ../grader ../engine < "$file" | tail -n 1
    fi
done
