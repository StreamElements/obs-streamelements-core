#!/usr/bin/env bash
#
# PostToolUse hook: clang-format the edited C/C++/ObjC++ file.
#
# Formats ONLY the line ranges that differ from HEAD, never the whole file.
# The tree was formatted with an older clang-format than most machines now
# have (VS 2022 ships 19.x, .clang-format targets 8+), so whole-file runs
# rewrite ~10% of lines in large files and bury real changes in churn.
#
# Vendored deps/ and committed CMake/MSBuild output are skipped outright.
#
set -u

input=$(cat)

# .tool_input.file_path, without depending on jq (not present in Git Bash).
file=$(printf '%s' "$input" |
	grep -oP '"file_path"\s*:\s*"\K(?:[^"\\]|\\.)*' | head -1)
[ -n "$file" ] || exit 0

# JSON-escaped Windows paths arrive as C:\\Shared\\... -> C:/Shared/...
file=$(printf '%s' "$file" | sed 's|\\\\|/|g')
[ -f "$file" ] || exit 0

case "$file" in
*.c | *.cc | *.cpp | *.h | *.hpp | *.m | *.mm) ;;
*) exit 0 ;;
esac

case "$file" in
*/deps/* | */CMakeFiles/* | */*_autogen/* | *qrc_streamelements.cpp) exit 0 ;;
esac

clang_format=""
for candidate in clang-format clang-format-19 clang-format-12 clang-format-10; do
	if command -v "$candidate" >/dev/null 2>&1; then
		clang_format=$(command -v "$candidate")
		break
	fi
done
if [ -z "$clang_format" ]; then
	for candidate in \
		"/c/Program Files/Microsoft Visual Studio/2022"/*/VC/Tools/Llvm/bin/clang-format.exe \
		"/c/Program Files/LLVM/bin/clang-format.exe"; do
		if [ -x "$candidate" ]; then
			clang_format="$candidate"
			break
		fi
	done
fi
[ -n "$clang_format" ] || exit 0

# Resolve the repo from the edited file, not from $0 or the cwd: on Windows
# $0 arrives as a backslash path, where dirname yields ".".
repo=$(cd "$(dirname "$file")" 2>/dev/null && git rev-parse --show-toplevel 2>/dev/null)
[ -n "$repo" ] || exit 0
cd "$repo" || exit 0

if git ls-files --error-unmatch -- "$file" >/dev/null 2>&1; then
	# Tracked: format only the hunks that differ from HEAD.
	ranges=$(git diff -U0 --no-color -- "$file" |
		grep -oP '^@@ -\S+ \+\K[0-9]+(,[0-9]+)?' |
		awk -F, '{ n = ($2 == "" ? 1 : $2);
		           if (n > 0) printf "--lines=%d:%d ", $1, $1 + n - 1 }')
	[ -n "$ranges" ] || exit 0
else
	# Untracked: brand new file, format all of it.
	ranges=""
fi

# shellcheck disable=SC2086
"$clang_format" -i -style=file -fallback-style=none $ranges "$file"
