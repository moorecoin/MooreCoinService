this directory contains example code that uses protocol buffers to manage an
address book.  two programs are provided, each with three different
implementations, one written in each of c++, java, and python.  the add_person
example adds a new person to an address book, prompting the user to input
the person's information.  the list_people example lists people already in the
address book.  the examples use the exact same format in all three languages,
so you can, for example, use add_person_java to create an address book and then
use list_people_python to read it.

you must install the protobuf package before you can build these.

to build all the examples (on a unix-like system), simply run "make".  this
creates the following executable files in the current directory:
  add_person_cpp     list_people_cpp
  add_person_java    list_people_java
  add_person_python  list_people_python

if you only want to compile examples in one language, use "make cpp"*,
"make java", or "make python".

all of these programs simply take an address book file as their parameter.
the add_person programs will create the file if it doesn't already exist.

these examples are part of the protocol buffers tutorial, located at:
  http://code.google.com/apis/protocolbuffers/docs/tutorials.html

* note that on some platforms you may have to edit the makefile and remove
"-lpthread" from the linker commands (perhaps replacing it with something else).
we didn't do this automatically because we wanted to keep the example simple.
