#!/bin/bash

# exit on error.
set -e

if [ -n "$use_clang" ]; then
  echo "error: coverage test is supported only for gcc."
  exit 1
fi

root=".."
# fetch right version of gcov
if [ -d /mnt/gvfs/third-party -a -z "$cxx" ]; then
  source $root/build_tools/fbcode.gcc471.sh
  gcov=$toolchain_executables/gcc/gcc-4.7.1/cc6c9dc/bin/gcov
else
  gcov=$(which gcov)
fi

coverage_dir="$pwd/coverage_report"
mkdir -p $coverage_dir

# find all gcno files to generate the coverage report

gcno_files=`find $root -name "*.gcno"`
$gcov --preserve-paths --relative-only --no-output $gcno_files 2>/dev/null |
  # parse the raw gcov report to more human readable form.
  python $root/coverage/parse_gcov_output.py |
  # write the output to both stdout and report file.
  tee $coverage_dir/coverage_report_all.txt &&
echo -e "generated coverage report for all files: $coverage_dir/coverage_report_all.txt\n"

# todo: we also need to get the files of the latest commits.
# get the most recently committed files.
latest_files=`
  git show --pretty="format:" --name-only head |
  grep -v "^$" |
  paste -s -d,`
recent_report=$coverage_dir/coverage_report_recent.txt

echo -e "recently updated files: $latest_files\n" > $recent_report
$gcov --preserve-paths --relative-only --no-output $gcno_files 2>/dev/null |
  python $root/coverage/parse_gcov_output.py -interested-files $latest_files |
  tee -a $recent_report &&
echo -e "generated coverage report for recently updated files: $recent_report\n"

# unless otherwise specified, we'll not generate html report by default
if [ -z "$html" ]; then
  exit 0
fi

# generate the html report. if we cannot find lcov in this machine, we'll simply
# skip this step.
echo "generating the html coverage report..."

lcov=$(which lcov || true 2>/dev/null)
if [ -z $lcov ]
then
  echo "skip: cannot find lcov to generate the html report."
  exit 0
fi

lcov_version=$(lcov -v | grep 1.1 || true)
if [ $lcov_version ]
then
  echo "not supported lcov version. expect lcov 1.1."
  exit 0
fi

(cd $root; lcov --no-external \
     --capture  \
     --directory $pwd \
     --gcov-tool $gcov \
     --output-file $coverage_dir/coverage.info)

genhtml $coverage_dir/coverage.info -o $coverage_dir

echo "html coverage report is generated in $coverage_dir"
