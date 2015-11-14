## compilation

rocksdb's library should be able to compile without any dependency installed,
although we recommend installing some compression libraries (see below).
we do depend on newer gcc with c++11 support.

there are few options when compiling rocksdb:

* [recommended] `make static_lib` will compile librocksdb.a, rocksdb static library.

* `make shared_lib` will compile librocksdb.so, rocksdb shared library.

* `make check` will compile and run all the unit tests

* `make all` will compile our static library, and all our tools and unit tests. our tools
depend on gflags. you will need to have gflags installed to run `make all`.

## dependencies

* you can link rocksdb with following compression libraries:
  - [zlib](http://www.zlib.net/) - a library for data compression.
  - [bzip2](http://www.bzip.org/) - a library for data compression.
  - [snappy](https://code.google.com/p/snappy/) - a library for fast
      data compression.

* all our tools depend on:
  - [gflags](https://code.google.com/p/gflags/) - a library that handles
      command line flags processing. you can compile rocksdb library even
      if you don't have gflags installed.

## supported platforms

* **linux - ubuntu**
    * upgrade your gcc to version at least 4.7 to get c++11 support.
    * install gflags. first, try: `sudo apt-get install libgflags-dev`
      if this doesn't work and you're using ubuntu, here's a nice tutorial:
      (http://askubuntu.com/questions/312173/installing-gflags-12-04)
    * install snappy. this is usually as easy as:
      `sudo apt-get install libsnappy-dev`.
    * install zlib. try: `sudo apt-get install zlib1g-dev`.
    * install bzip2: `sudo apt-get install libbz2-dev`.
* **linux - centos**
    * upgrade your gcc to version at least 4.7 to get c++11 support:
      `yum install gcc47-c++`
    * install gflags:

              wget https://gflags.googlecode.com/files/gflags-2.0-no-svn-files.tar.gz
              tar -xzvf gflags-2.0-no-svn-files.tar.gz
              cd gflags-2.0
              ./configure && make && sudo make install

    * install snappy:

              wget https://snappy.googlecode.com/files/snappy-1.1.1.tar.gz
              tar -xzvf snappy-1.1.1.tar.gz
              cd snappy-1.1.1
              ./configure && make && sudo make install

    * install zlib:

              sudo yum install zlib
              sudo yum install zlib-devel

    * install bzip2:

              sudo yum install bzip2
              sudo yum install bzip2-devel

* **os x**:
    * install latest c++ compiler that supports c++ 11:
        * update xcode:  run `xcode-select --install` (or install it from xcode app's settting).
        * install via [homebrew](http://brew.sh/).
            * if you're first time developer in macos, you still need to run: `xcode-select --install` in your command line.
            * run `brew tap homebrew/dupes; brew install gcc47 --use-llvm` to install gcc 4.7 (or higher).
    * install zlib, bzip2 and snappy libraries for compression.
    * install gflags. we have included a script
    `build_tools/mac-install-gflags.sh`, which should automatically install it (execute this file instead of runing using "source" command).
    if you installed gflags by other means (for example, `brew install gflags`),
    please set `library_path` and `cpath` accordingly.
    * please note that some of the optimizations/features are disabled in osx.
    we did not run any production workloads on it.

* **ios**:
  * run: `target_os=ios make static_lib`
