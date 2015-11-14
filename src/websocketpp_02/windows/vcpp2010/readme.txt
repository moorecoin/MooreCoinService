build boost
===========

prerequisites: visual sutdio c ++ 2010 express (or higher)

download boost from http://www.boost.org/

unzip boost_1_47_0.zip to c:\boost_1_47_0

open a "visual studio command prompt (2010)" (not a regular cmd.exe!).

cd c:\boost_1_47_0

bootstrap

.\b2 runtime-link=static

now set a system environment variable:

boostroot = c:\boost_1_47_0


background:

 - http://www.boost.org/doc/libs/1_47_0/more/getting_started/windows.html
 - http://www.boost.org/doc/libs/1_47_0/more/getting_started/windows.html#library-naming
 - http://stackoverflow.com/questions/2035287/static-runtime-library-linking-for-visual-c-express-2008


build websocket++
=================

open websocketpp.sln in vs.

build solution (f7).
