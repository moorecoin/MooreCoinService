#!/bin/bash
# if clang_format_diff.py command is not specfied, we assume we are able to
# access directly without any path.
if [ -z $clang_format_diff ]
then
clang_format_diff="clang-format-diff.py"
fi

# check clang-format-diff.py
if ! which $clang_format_diff &> /dev/null
then
  echo "you didn't have clang-format-diff.py available in your computer!"
  echo "you can download it by running: "
  echo "    curl http://goo.gl/iuw1u2"
  exit 128
fi

# check argparse, a library that clang-format-diff.py requires.
python 2>/dev/null << eof
import argparse
eof

if [ "$?" != 0 ]
then
  echo "to run clang-format-diff.py, we'll need the library "argparse" to be"
  echo "installed. you can try either of the follow ways to install it:"
  echo "  1. manually download argparse: https://pypi.python.org/pypi/argparse"
  echo "  2. easy_install argparse (if you have easy_install)"
  echo "  3. pip install argparse (if you have pip)"
  exit 129
fi

# todo(kailiu) following work is not complete since we still need to figure
# out how to add the modified files done pre-commit hook to git's commit index.
#
# check if this script has already been added to pre-commit hook.
# will suggest user to add this script to pre-commit hook if their pre-commit
# is empty.
# pre_commit_script_path="`git rev-parse --show-toplevel`/.git/hooks/pre-commit"
# if ! ls $pre_commit_script_path &> /dev/null
# then
#   echo "would you like to add this script to pre-commit hook, which will do "
#   echo -n "the format check for all the affected lines before you check in (y/n):"
#   read add_to_hook
#   if [ "$add_to_hook" == "y" ]
#   then
#     ln -s `git rev-parse --show-toplevel`/build_tools/format-diff.sh $pre_commit_script_path
#   fi
# fi
set -e

uncommitted_code=`git diff head`

# if there's no uncommitted changes, we assume user are doing post-commit
# format check, in which case we'll check the modified lines from latest commit.
# otherwise, we'll check format of the uncommitted code only.
if [ -z "$uncommitted_code" ]
then
  # check the format of last commit
  diffs=$(git diff -u0 head^ | $clang_format_diff -p 1)
else
  # check the format of uncommitted lines,
  diffs=$(git diff -u0 head | $clang_format_diff -p 1)
fi

if [ -z "$diffs" ]
then
  echo "nothing needs to be reformatted!"
  exit 0
fi

# highlight the insertion/deletion from the clang-format-diff.py's output
color_end="\033[0m"
color_red="\033[0;31m" 
color_green="\033[0;32m" 

echo -e "detect lines that doesn't follow the format rules:\r"
# add the color to the diff. lines added will be green; lines removed will be red.
echo "$diffs" | 
  sed -e "s/\(^-.*$\)/`echo -e \"$color_red\1$color_end\"`/" |
  sed -e "s/\(^+.*$\)/`echo -e \"$color_green\1$color_end\"`/"
echo -e "would you like to fix the format automatically (y/n): \c"

# make sure under any mode, we can read user input.
exec < /dev/tty
read to_fix

if [ "$to_fix" != "y" ]
then
  exit 1
fi

# do in-place format adjustment.
git diff -u0 head^ | $clang_format_diff -i -p 1
echo "files reformatted!"

# amend to last commit if user do the post-commit format check
if [ -z "$uncommitted_code" ]; then
  echo -e "would you like to amend the changes to last commit (`git log head --oneline | head -1`)? (y/n): \c"
  read to_amend

  if [ "$to_amend" == "y" ]
  then
    git commit -a --amend --reuse-message head
    echo "amended to last commit"
  fi
fi
