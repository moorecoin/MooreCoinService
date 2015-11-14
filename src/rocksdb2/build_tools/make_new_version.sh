#!/bin/bash
#  copyright (c) 2013, facebook, inc.  all rights reserved.
#  this source code is licensed under the bsd-style license found in the
#  license file in the root directory of this source tree. an additional grant
#  of patent rights can be found in the patents file in the same directory.

set -e
if [ -z "$git" ]
then
  git="git"
fi

# print out the colored progress info so that it can be brainlessly 
# distinguished by users.
function title() {
  echo -e "\033[1;32m$*\033[0m"
}

usage="create new rocksdb version and prepare it for the release process\n"
usage+="usage: ./make_new_version.sh <version>"

# -- pre-check
if [[ $# < 1 ]]; then
  echo -e $usage
  exit 1
fi

rocksdb_version=$1

git_branch=`git rev-parse --abbrev-ref head`
echo $git_branch

if [ $git_branch != "master" ]; then
  echo "error: current branch is '$git_branch', please switch to master branch."
  exit 1
fi

title "adding new tag for this release ..."
branch="$rocksdb_version.fb"
$git co -b $branch

# setting up the proxy for remote repo access
title "pushing new branch to remote repo ..."
git push origin --set-upstream $branch

title "branch $branch is pushed to github;"
