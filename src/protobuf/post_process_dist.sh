#! /bin/sh

# this script takes the result of "make dist" and:
# 1) unpacks it.
# 2) ensures all contents are user-writable.  some version control systems
#    keep code read-only until you explicitly ask to edit it, and the normal
#    "make dist" process does not correct for this, so the result is that
#    the entire dist is still marked read-only when unpacked, which is
#    annoying.  so, we fix it.
# 3) convert msvc project files to msvc 2005, so that anyone who has version
#    2005 *or* 2008 can open them.  (in version control, we keep things in
#    msvc 2008 format since that's what we use in development.)
# 4) uses the result to create .tar.gz, .tar.bz2, and .zip versions and
#    deposites them in the "dist" directory.  in the .zip version, all
#    non-testdata .txt files are converted to windows-style line endings.
# 5) cleans up after itself.

if [ "$1" == "" ]; then
  echo "usage:  $1 distfile" >&2
  exit 1
fi

if [ ! -e $1 ]; then
  echo $1": file not found." >&2
  exit 1
fi

set -ex

basename=`basename $1 .tar.gz`

# create a directory called "dist", copy the tarball there and unpack it.
mkdir dist
cp $1 dist
cd dist
tar zxvf $basename.tar.gz
rm $basename.tar.gz

# set the entire contents to be user-writable.
chmod -r u+w $basename

# convert the msvc projects to msvc 2005 format.
cd $basename/vsprojects
./convert2008to2005.sh
cd ..

# build the dist again in .tar.gz and .tar.bz2 formats.
./configure
make dist-gzip
make dist-bzip2

# convert all text files to use dos-style line endings, then build a .zip
# distribution.
todos *.txt */*.txt
make dist-zip

# clean up.
mv $basename.tar.gz $basename.tar.bz2 $basename.zip ..
cd ..
rm -rf $basename
