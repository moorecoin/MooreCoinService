contents
--------

this folder contains three kinds of file:

- code, such as protobench.java, to build the benchmarking framework.
- protocol buffer definitions (.proto files)
- sample data files

if we end up with a lot of different benchmarks it may be worth
separating these out info different directories, but while there are
so few they might as well all be together.

running a benchmark (java)
--------------------------

1) build protoc and the java protocol buffer library. the examples
   below assume a jar file (protobuf.jar) has been built and copied
   into this directory.

2) build protobench:
   $ javac -d tmp -cp protobuf.jar protobench.java
   
3) generate code for the relevant benchmark protocol buffer, e.g.
   $ protoc --java_out=tmp google_size.proto google_speed.proto
   
4) build the generated code, e.g.
   $ cd tmp
   $ javac -d . -cp ../protobuf.jar benchmarks/*.java
           
5) run the test. arguments are given in pairs - the first argument
   is the descriptor type; the second is the filename. for example:
   $ java -cp .;../protobuf.jar com.google.protocolbuffers.protobench
          benchmarks.googlesize$sizemessage1 ../google_message1.dat
          benchmarks.googlespeed$speedmessage1 ../google_message1.dat
          benchmarks.googlesize$sizemessage2 ../google_message2.dat
          benchmarks.googlespeed$speedmessage2 ../google_message2.dat
          
6) wait! each test runs for around 30 seconds, and there are 6 tests
   per class/data combination. the above command would therefore take
   about 12 minutes to run.

   
benchmarks available
--------------------

from google:
google_size.proto and google_speed.proto, messages
google_message1.dat and google_message2.dat. the proto files are
equivalent, but optimized differently.
