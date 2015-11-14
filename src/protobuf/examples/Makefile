# see readme.txt.

.phony: all cpp java python clean

all: cpp java python

cpp:    add_person_cpp    list_people_cpp
java:   add_person_java   list_people_java
python: add_person_python list_people_python

clean:
	rm -f add_person_cpp list_people_cpp add_person_java list_people_java add_person_python list_people_python
	rm -f javac_middleman addperson*.class listpeople*.class com/example/tutorial/*.class
	rm -f protoc_middleman addressbook.pb.cc addressbook.pb.h addressbook_pb2.py com/example/tutorial/addressbookprotos.java
	rm -f *.pyc
	rmdir com/example/tutorial 2>/dev/null || true
	rmdir com/example 2>/dev/null || true
	rmdir com 2>/dev/null || true

protoc_middleman: addressbook.proto
	protoc --cpp_out=. --java_out=. --python_out=. addressbook.proto
	@touch protoc_middleman

add_person_cpp: add_person.cc protoc_middleman
	pkg-config --cflags protobuf  # fails if protobuf is not installed
	c++ add_person.cc addressbook.pb.cc -o add_person_cpp `pkg-config --cflags --libs protobuf`

list_people_cpp: list_people.cc protoc_middleman
	pkg-config --cflags protobuf  # fails if protobuf is not installed
	c++ list_people.cc addressbook.pb.cc -o list_people_cpp `pkg-config --cflags --libs protobuf`

javac_middleman: addperson.java listpeople.java protoc_middleman
	javac addperson.java listpeople.java com/example/tutorial/addressbookprotos.java
	@touch javac_middleman

add_person_java: javac_middleman
	@echo "writing shortcut script add_person_java..."
	@echo '#! /bin/sh' > add_person_java
	@echo 'java -classpath .:$$classpath addperson "$$@"' >> add_person_java
	@chmod +x add_person_java

list_people_java: javac_middleman
	@echo "writing shortcut script list_people_java..."
	@echo '#! /bin/sh' > list_people_java
	@echo 'java -classpath .:$$classpath listpeople "$$@"' >> list_people_java
	@chmod +x list_people_java

add_person_python: add_person.py protoc_middleman
	@echo "writing shortcut script add_person_python..."
	@echo '#! /bin/sh' > add_person_python
	@echo './add_person.py "$$@"' >> add_person_python
	@chmod +x add_person_python

list_people_python: list_people.py protoc_middleman
	@echo "writing shortcut script list_people_python..."
	@echo '#! /bin/sh' > list_people_python
	@echo './list_people.py "$$@"' >> list_people_python
	@chmod +x list_people_python
