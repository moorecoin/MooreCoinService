#! /bin/sh -e

# this script downgrades msvc 2008 projects to msvc 2005 projects, allowing
# people with msvc 2005 to open them.  otherwise, msvc 2005 simply refuses to
# open projects created with 2008.  we run this as part of our release process.
# if you obtained the code direct from version control and you want to use
# msvc 2005, you may have to run this manually.  (hint:  use cygwin or msys.)

for file in *.sln; do
  echo "downgrading $file..."
  sed -i -re 's/format version 10.00/format version 9.00/g;
              s/visual studio 2008/visual studio 2005/g;' $file
done

for file in *.vcproj; do
  echo "downgrading $file..."
  sed -i -re 's/version="9.00"/version="8.00"/g;' $file
done

# yes, really, that's it.
