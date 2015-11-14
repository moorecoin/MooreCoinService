protocol buffers - google's data interchange format
copyright 2008 google inc.

this directory contains the java protocol buffers runtime library.

installation - with maven
=========================

the protocol buffers build is managed using maven.  if you would
rather build without maven, see below.

1) install apache maven if you don't have it:

     http://maven.apache.org/

2) build the c++ code, or obtain a binary distribution of protoc.  if
   you install a binary distribution, make sure that it is the same
   version as this package.  if in doubt, run:

     $ protoc --version

   you will need to place the protoc executable in ../src.  (if you
   built it yourself, it should already be there.)

3) run the tests:

     $ mvn test

   if some tests fail, this library may not work correctly on your
   system.  continue at your own risk.

4) install the library into your maven repository:

     $ mvn install

5) if you do not use maven to manage your own build, you can build a
   .jar file to use:

     $ mvn package

   the .jar will be placed in the "target" directory.

installation - 'lite' version - with maven
==========================================

building the 'lite' version of the java protocol buffers library is
the same as building the full version, except that all commands are
run using the 'lite' profile.  (see
http://maven.apache.org/guides/introduction/introduction-to-profiles.html)

e.g. to install the lite version of the jar, you would run:

  $ mvn install -p lite

the resulting artifact has the 'lite' classifier.  to reference it
for dependency resolution, you would specify it as:

  <dependency>
    <groupid>com.google.protobuf</groupid>
    <artifactid>protobuf-java</artifactid>
    <version>${version}</version>
    <classifier>lite</classifier>
  </dependency>

installation - without maven
============================

if you would rather not install maven to build the library, you may
follow these instructions instead.  note that these instructions skip
running unit tests.

1) build the c++ code, or obtain a binary distribution of protoc.  if
   you install a binary distribution, make sure that it is the same
   version as this package.  if in doubt, run:

     $ protoc --version

   if you built the c++ code without installing, the compiler binary
   should be located in ../src.

2) invoke protoc to build descriptorprotos.java:

     $ protoc --java_out=src/main/java -i../src \
         ../src/google/protobuf/descriptor.proto

3) compile the code in src/main/java using whatever means you prefer.

4) install the classes wherever you prefer.

usage
=====

the complete documentation for protocol buffers is available via the
web at:

  http://code.google.com/apis/protocolbuffers/
