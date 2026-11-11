#!/bin/bash
# Run the checker inside WSL.
# The Windows path contains a space, which breaks the Makefiles,
# so the project is copied to ~/memory-allocator first.
#
#   ./run-tests.sh                 -> full check (make check, with Valgrind)
#   ./run-tests.sh -d              -> all tests without Valgrind
#   ./run-tests.sh <test-name> -v  -> one test, verbose diff

set -e

SRC="$(cd "$(dirname "$0")" && pwd)"
DST="$HOME/memory-allocator"

rsync -a --delete --exclude .git --exclude screenshots "$SRC/" "$DST/"
cd "$DST/tests"

if [ $# -eq 0 ]; then
	make check
else
	make clean_src clean_snippets src snippets > /dev/null
	python3 run_tests.py "$@"
fi
