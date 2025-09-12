#!/usr/bin/env bash

# This script applies clang-format to the whole repository.

# Check version
version_string=$(clang-format --version | sed -E 's/^.*(\d+\.\d+\.\d+-.*).*$/\1/')

# Get all C++ files checked into the repo, excluding submodules
root_folder=$(git rev-parse --show-toplevel)
all_files=$( \
    git ls-tree --full-tree -r --name-only HEAD . \
    | grep "^src/\(colmap\|pycolmap\).*\(\.cc\|\.h\|\.hpp\|\.cpp\|\.cu\)$" \
    | sed "s~^~$root_folder/~")
num_files=$(echo $all_files | wc -w)
echo "Formatting ${num_files} files"

for file in $all_files; do
    clang-format -i "$file"
done