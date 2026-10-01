#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
ARCHIVE='@ARCHIVE@'
DESTINATION="$ROOT/@NAME@-extracted"
if [ -e "$DESTINATION" ] || [ -e "$ARCHIVE" ]; then
    echo "Extraction destination or temporary archive already exists." >&2
    exit 1
fi
echo "Checking downloaded files..."
shasum -a 256 -c '@NAME@.files.sha256'
echo "Joining archive parts..."
part=1
while [ "$part" -le @COUNT@ ]; do
    file=$(printf '%s.%03d' "$ARCHIVE" "$part")
    cat "$file" >> "$ARCHIVE"
    part=$((part + 1))
done
mkdir "$DESTINATION"
ditto -x -k "$ARCHIVE" "$DESTINATION"
rm "$ARCHIVE"
echo "Ready: $DESTINATION/@TOP@"
