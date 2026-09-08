#!/bin/bash
##############################################################################
# Copyright (c) 2026
#
# Author(s):
#  Peter L Jones <peter.l.jones.dymlyd@gmail.com>
#  The Jamulus Development Team
#
##############################################################################
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
##############################################################################

# Prepare the single Android App Bundle for the Google Play upload action.

set -euo pipefail

INPUT_DIR="${1:-deploy}"
OUTPUT_DIR="${2:-play-store}"

mapfile -t AABS < <(find "$INPUT_DIR" -maxdepth 1 -type f -name '*.aab' -print)

if [[ ${#AABS[@]} -ne 1 ]]; then
    printf 'Expected exactly one Android App Bundle in %s, found %s\n' \
        "$INPUT_DIR" "${#AABS[@]}" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
OUTPUT_FILE="$OUTPUT_DIR/Jamulus.aab"
cp -- "${AABS[0]}" "$OUTPUT_FILE"

if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
    printf 'release_file=%s\n' "$OUTPUT_FILE" >> "$GITHUB_OUTPUT"
else
    printf '%s\n' "$OUTPUT_FILE"
fi
