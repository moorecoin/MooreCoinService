lz4 - extremely fast compression
================================

lz4 is lossless compression algorithm, providing compression speed at 400 mb/s per core, scalable with multi-cores cpu. it also features an extremely fast decoder, with speed in multiple gb/s per core, typically reaching ram speed limits on multi-core systems.
a high compression derivative, called lz4_hc, is also provided. it trades cpu time for compression ratio.

|branch      |status   |
|------------|---------|
|master      | [![build status](https://travis-ci.org/cyan4973/lz4.svg?branch=master)](https://travis-ci.org/cyan4973/lz4) |
|dev         | [![build status](https://travis-ci.org/cyan4973/lz4.svg?branch=dev)](https://travis-ci.org/cyan4973/lz4) |

this is an official mirror of lz4 project, [hosted on google code](http://code.google.com/p/lz4/).
the intention is to offer github's capabilities to lz4 users, such as cloning, branch, pull requests or source download.

the "master" branch will reflect, the status of lz4 at its official homepage.
the "dev" branch is the one where all contributions will be merged. if you plan to propose a patch, please commit into the "dev" branch. direct commit to "master" are not permitted.
feature branches will also exist, typically to introduce new requirements, and be temporarily available for testing before merge into "dev" branch.


benchmarks
-------------------------

the benchmark uses the [open-source benchmark program by m^2 (v0.14.2)](http://encode.ru/threads/1371-filesystem-benchmark?p=33548&viewfull=1#post33548) compiled with gcc v4.6.1 on linux ubuntu 64-bits v11.10,
the reference system uses a core i5-3340m @2.7ghz.
benchmark evaluates the compression of reference [silesia corpus](http://sun.aei.polsl.pl/~sdeor/index.php?page=silesia) in single-thread mode.

<table>
  <tr>
    <th>compressor</th><th>ratio</th><th>compression</th><th>decompression</th>
  </tr>
  <tr>
    <th>lz4 (r101)</th><th>2.084</th><th>422 mb/s</th><th>1820 mb/s</th>
  </tr>
  <tr>
    <th>lzo 2.06</th><th>2.106</th><th>414 mb/s</th><th>600 mb/s</th>
  </tr>
  <tr>
    <th>quicklz 1.5.1b6</th><th>2.237</th><th>373 mb/s</th><th>420 mb/s</th>
  </tr>
  <tr>
    <th>snappy 1.1.0</th><th>2.091</th><th>323 mb/s</th><th>1070 mb/s</th>
  </tr>
  <tr>
    <th>lzf</th><th>2.077</th><th>270 mb/s</th><th>570 mb/s</th>
  </tr>
  <tr>
    <th>zlib 1.2.8 -1</th><th>2.730</th><th>65 mb/s</th><th>280 mb/s</th>
  </tr>
  <tr>
    <th>lz4 hc (r101)</th><th>2.720</th><th>25 mb/s</th><th>2080 mb/s</th>
  </tr>
  <tr>
    <th>zlib 1.2.8 -6</th><th>3.099</th><th>21 mb/s</th><th>300 mb/s</th>
  </tr>
</table>

