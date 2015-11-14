// copyright 2005 and onwards google inc.
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

#include <math.h>
#include <stdlib.h>


#include <algorithm>
#include <string>
#include <vector>

#include "snappy.h"
#include "snappy-internal.h"
#include "snappy-test.h"
#include "snappy-sinksource.h"

define_int32(start_len, -1,
             "starting prefix size for testing (-1: just full file contents)");
define_int32(end_len, -1,
             "starting prefix size for testing (-1: just full file contents)");
define_int32(bytes, 10485760,
             "how many bytes to compress/uncompress per file for timing");

define_bool(zlib, false,
            "run zlib compression (http://www.zlib.net)");
define_bool(lzo, false,
            "run lzo compression (http://www.oberhumer.com/opensource/lzo/)");
define_bool(quicklz, false,
            "run quicklz compression (http://www.quicklz.com/)");
define_bool(liblzf, false,
            "run liblzf compression "
            "(http://www.goof.com/pcg/marc/liblzf.html)");
define_bool(fastlz, false,
            "run fastlz compression (http://www.fastlz.org/");
define_bool(snappy, true, "run snappy compression");


define_bool(write_compressed, false,
            "write compressed versions of each file to <file>.comp");
define_bool(write_uncompressed, false,
            "write uncompressed versions of each file to <file>.uncomp");

namespace snappy {


#ifdef have_func_mmap

// to test against code that reads beyond its input, this class copies a
// string to a newly allocated group of pages, the last of which
// is made unreadable via mprotect. note that we need to allocate the
// memory with mmap(), as posix allows mprotect() only on memory allocated
// with mmap(), and some malloc/posix_memalign implementations expect to
// be able to read previously allocated memory while doing heap allocations.
class dataendingatunreadablepage {
 public:
  explicit dataendingatunreadablepage(const string& s) {
    const size_t page_size = getpagesize();
    const size_t size = s.size();
    // round up space for string to a multiple of page_size.
    size_t space_for_string = (size + page_size - 1) & ~(page_size - 1);
    alloc_size_ = space_for_string + page_size;
    mem_ = mmap(null, alloc_size_,
                prot_read|prot_write, map_private|map_anonymous, -1, 0);
    check_ne(map_failed, mem_);
    protected_page_ = reinterpret_cast<char*>(mem_) + space_for_string;
    char* dst = protected_page_ - size;
    memcpy(dst, s.data(), size);
    data_ = dst;
    size_ = size;
    // make guard page unreadable.
    check_eq(0, mprotect(protected_page_, page_size, prot_none));
  }

  ~dataendingatunreadablepage() {
    // undo the mprotect.
    check_eq(0, mprotect(protected_page_, getpagesize(), prot_read|prot_write));
    check_eq(0, munmap(mem_, alloc_size_));
  }

  const char* data() const { return data_; }
  size_t size() const { return size_; }

 private:
  size_t alloc_size_;
  void* mem_;
  char* protected_page_;
  const char* data_;
  size_t size_;
};

#else  // have_func_mmap

// fallback for systems without mmap.
typedef string dataendingatunreadablepage;

#endif

enum compressortype {
  zlib, lzo, liblzf, quicklz, fastlz, snappy
};

const char* names[] = {
  "zlib", "lzo", "liblzf", "quicklz", "fastlz", "snappy"
};

static size_t minimumrequiredoutputspace(size_t input_size,
                                         compressortype comp) {
  switch (comp) {
#ifdef zlib_version
    case zlib:
      return zlib::mincompressbufsize(input_size);
#endif  // zlib_version

#ifdef lzo_version
    case lzo:
      return input_size + input_size/64 + 16 + 3;
#endif  // lzo_version

#ifdef lzf_version
    case liblzf:
      return input_size;
#endif  // lzf_version

#ifdef qlz_version_major
    case quicklz:
      return input_size + 36000;  // 36000 is used for scratch.
#endif  // qlz_version_major

#ifdef fastlz_version
    case fastlz:
      return max(static_cast<int>(ceil(input_size * 1.05)), 66);
#endif  // fastlz_version

    case snappy:
      return snappy::maxcompressedlength(input_size);

    default:
      log(fatal) << "unknown compression type number " << comp;
  }
}

// returns true if we successfully compressed, false otherwise.
//
// if compressed_is_preallocated is set, do not resize the compressed buffer.
// this is typically what you want for a benchmark, in order to not spend
// time in the memory allocator. if you do set this flag, however,
// "compressed" must be preinitialized to at least mincompressbufsize(comp)
// number of bytes, and may contain junk bytes at the end after return.
static bool compress(const char* input, size_t input_size, compressortype comp,
                     string* compressed, bool compressed_is_preallocated) {
  if (!compressed_is_preallocated) {
    compressed->resize(minimumrequiredoutputspace(input_size, comp));
  }

  switch (comp) {
#ifdef zlib_version
    case zlib: {
      zlib zlib;
      ulongf destlen = compressed->size();
      int ret = zlib.compress(
          reinterpret_cast<bytef*>(string_as_array(compressed)),
          &destlen,
          reinterpret_cast<const bytef*>(input),
          input_size);
      check_eq(z_ok, ret);
      if (!compressed_is_preallocated) {
        compressed->resize(destlen);
      }
      return true;
    }
#endif  // zlib_version

#ifdef lzo_version
    case lzo: {
      unsigned char* mem = new unsigned char[lzo1x_1_15_mem_compress];
      lzo_uint destlen;
      int ret = lzo1x_1_15_compress(
          reinterpret_cast<const uint8*>(input),
          input_size,
          reinterpret_cast<uint8*>(string_as_array(compressed)),
          &destlen,
          mem);
      check_eq(lzo_e_ok, ret);
      delete[] mem;
      if (!compressed_is_preallocated) {
        compressed->resize(destlen);
      }
      break;
    }
#endif  // lzo_version

#ifdef lzf_version
    case liblzf: {
      int destlen = lzf_compress(input,
                                 input_size,
                                 string_as_array(compressed),
                                 input_size);
      if (destlen == 0) {
        // lzf *can* cause lots of blowup when compressing, so they
        // recommend to limit outsize to insize, and just not compress
        // if it's bigger.  ideally, we'd just swap input and output.
        compressed->assign(input, input_size);
        destlen = input_size;
      }
      if (!compressed_is_preallocated) {
        compressed->resize(destlen);
      }
      break;
    }
#endif  // lzf_version

#ifdef qlz_version_major
    case quicklz: {
      qlz_state_compress *state_compress = new qlz_state_compress;
      int destlen = qlz_compress(input,
                                 string_as_array(compressed),
                                 input_size,
                                 state_compress);
      delete state_compress;
      check_ne(0, destlen);
      if (!compressed_is_preallocated) {
        compressed->resize(destlen);
      }
      break;
    }
#endif  // qlz_version_major

#ifdef fastlz_version
    case fastlz: {
      // use level 1 compression since we mostly care about speed.
      int destlen = fastlz_compress_level(
          1,
          input,
          input_size,
          string_as_array(compressed));
      if (!compressed_is_preallocated) {
        compressed->resize(destlen);
      }
      check_ne(destlen, 0);
      break;
    }
#endif  // fastlz_version

    case snappy: {
      size_t destlen;
      snappy::rawcompress(input, input_size,
                          string_as_array(compressed),
                          &destlen);
      check_le(destlen, snappy::maxcompressedlength(input_size));
      if (!compressed_is_preallocated) {
        compressed->resize(destlen);
      }
      break;
    }


    default: {
      return false;     // the asked-for library wasn't compiled in
    }
  }
  return true;
}

static bool uncompress(const string& compressed, compressortype comp,
                       int size, string* output) {
  switch (comp) {
#ifdef zlib_version
    case zlib: {
      output->resize(size);
      zlib zlib;
      ulongf destlen = output->size();
      int ret = zlib.uncompress(
          reinterpret_cast<bytef*>(string_as_array(output)),
          &destlen,
          reinterpret_cast<const bytef*>(compressed.data()),
          compressed.size());
      check_eq(z_ok, ret);
      check_eq(static_cast<ulongf>(size), destlen);
      break;
    }
#endif  // zlib_version

#ifdef lzo_version
    case lzo: {
      output->resize(size);
      lzo_uint destlen;
      int ret = lzo1x_decompress(
          reinterpret_cast<const uint8*>(compressed.data()),
          compressed.size(),
          reinterpret_cast<uint8*>(string_as_array(output)),
          &destlen,
          null);
      check_eq(lzo_e_ok, ret);
      check_eq(static_cast<lzo_uint>(size), destlen);
      break;
    }
#endif  // lzo_version

#ifdef lzf_version
    case liblzf: {
      output->resize(size);
      int destlen = lzf_decompress(compressed.data(),
                                   compressed.size(),
                                   string_as_array(output),
                                   output->size());
      if (destlen == 0) {
        // this error probably means we had decided not to compress,
        // and thus have stored input in output directly.
        output->assign(compressed.data(), compressed.size());
        destlen = compressed.size();
      }
      check_eq(destlen, size);
      break;
    }
#endif  // lzf_version

#ifdef qlz_version_major
    case quicklz: {
      output->resize(size);
      qlz_state_decompress *state_decompress = new qlz_state_decompress;
      int destlen = qlz_decompress(compressed.data(),
                                   string_as_array(output),
                                   state_decompress);
      delete state_decompress;
      check_eq(destlen, size);
      break;
    }
#endif  // qlz_version_major

#ifdef fastlz_version
    case fastlz: {
      output->resize(size);
      int destlen = fastlz_decompress(compressed.data(),
                                      compressed.length(),
                                      string_as_array(output),
                                      size);
      check_eq(destlen, size);
      break;
    }
#endif  // fastlz_version

    case snappy: {
      snappy::rawuncompress(compressed.data(), compressed.size(),
                            string_as_array(output));
      break;
    }


    default: {
      return false;     // the asked-for library wasn't compiled in
    }
  }
  return true;
}

static void measure(const char* data,
                    size_t length,
                    compressortype comp,
                    int repeats,
                    int block_size) {
  // run tests a few time and pick median running times
  static const int kruns = 5;
  double ctime[kruns];
  double utime[kruns];
  int compressed_size = 0;

  {
    // chop the input into blocks
    int num_blocks = (length + block_size - 1) / block_size;
    vector<const char*> input(num_blocks);
    vector<size_t> input_length(num_blocks);
    vector<string> compressed(num_blocks);
    vector<string> output(num_blocks);
    for (int b = 0; b < num_blocks; b++) {
      int input_start = b * block_size;
      int input_limit = min<int>((b+1)*block_size, length);
      input[b] = data+input_start;
      input_length[b] = input_limit-input_start;

      // pre-grow the output buffer so we don't measure string append time.
      compressed[b].resize(minimumrequiredoutputspace(block_size, comp));
    }

    // first, try one trial compression to make sure the code is compiled in
    if (!compress(input[0], input_length[0], comp, &compressed[0], true)) {
      log(warning) << "skipping " << names[comp] << ": "
                   << "library not compiled in";
      return;
    }

    for (int run = 0; run < kruns; run++) {
      cycletimer ctimer, utimer;

      for (int b = 0; b < num_blocks; b++) {
        // pre-grow the output buffer so we don't measure string append time.
        compressed[b].resize(minimumrequiredoutputspace(block_size, comp));
      }

      ctimer.start();
      for (int b = 0; b < num_blocks; b++)
        for (int i = 0; i < repeats; i++)
          compress(input[b], input_length[b], comp, &compressed[b], true);
      ctimer.stop();

      // compress once more, with resizing, so we don't leave junk
      // at the end that will confuse the decompressor.
      for (int b = 0; b < num_blocks; b++) {
        compress(input[b], input_length[b], comp, &compressed[b], false);
      }

      for (int b = 0; b < num_blocks; b++) {
        output[b].resize(input_length[b]);
      }

      utimer.start();
      for (int i = 0; i < repeats; i++)
        for (int b = 0; b < num_blocks; b++)
          uncompress(compressed[b], comp, input_length[b], &output[b]);
      utimer.stop();

      ctime[run] = ctimer.get();
      utime[run] = utimer.get();
    }

    compressed_size = 0;
    for (int i = 0; i < compressed.size(); i++) {
      compressed_size += compressed[i].size();
    }
  }

  sort(ctime, ctime + kruns);
  sort(utime, utime + kruns);
  const int med = kruns/2;

  float comp_rate = (length / ctime[med]) * repeats / 1048576.0;
  float uncomp_rate = (length / utime[med]) * repeats / 1048576.0;
  string x = names[comp];
  x += ":";
  string urate = (uncomp_rate >= 0)
                 ? stringprintf("%.1f", uncomp_rate)
                 : string("?");
  printf("%-7s [b %dm] bytes %6d -> %6d %4.1f%%  "
         "comp %5.1f mb/s  uncomp %5s mb/s\n",
         x.c_str(),
         block_size/(1<<20),
         static_cast<int>(length), static_cast<uint32>(compressed_size),
         (compressed_size * 100.0) / max<int>(1, length),
         comp_rate,
         urate.c_str());
}


static int verifystring(const string& input) {
  string compressed;
  dataendingatunreadablepage i(input);
  const size_t written = snappy::compress(i.data(), i.size(), &compressed);
  check_eq(written, compressed.size());
  check_le(compressed.size(),
           snappy::maxcompressedlength(input.size()));
  check(snappy::isvalidcompressedbuffer(compressed.data(), compressed.size()));

  string uncompressed;
  dataendingatunreadablepage c(compressed);
  check(snappy::uncompress(c.data(), c.size(), &uncompressed));
  check_eq(uncompressed, input);
  return uncompressed.size();
}


static void verifyiovec(const string& input) {
  string compressed;
  dataendingatunreadablepage i(input);
  const size_t written = snappy::compress(i.data(), i.size(), &compressed);
  check_eq(written, compressed.size());
  check_le(compressed.size(),
           snappy::maxcompressedlength(input.size()));
  check(snappy::isvalidcompressedbuffer(compressed.data(), compressed.size()));

  // try uncompressing into an iovec containing a random number of entries
  // ranging from 1 to 10.
  char* buf = new char[input.size()];
  acmrandom rnd(input.size());
  int num = rnd.next() % 10 + 1;
  if (input.size() < num) {
    num = input.size();
  }
  struct iovec* iov = new iovec[num];
  int used_so_far = 0;
  for (int i = 0; i < num; ++i) {
    iov[i].iov_base = buf + used_so_far;
    if (i == num - 1) {
      iov[i].iov_len = input.size() - used_so_far;
    } else {
      // randomly choose to insert a 0 byte entry.
      if (rnd.onein(5)) {
        iov[i].iov_len = 0;
      } else {
        iov[i].iov_len = rnd.uniform(input.size());
      }
    }
    used_so_far += iov[i].iov_len;
  }
  check(snappy::rawuncompresstoiovec(
      compressed.data(), compressed.size(), iov, num));
  check(!memcmp(buf, input.data(), input.size()));
  delete[] iov;
  delete[] buf;
}

// test that data compressed by a compressor that does not
// obey block sizes is uncompressed properly.
static void verifynonblockedcompression(const string& input) {
  if (input.length() > snappy::kblocksize) {
    // we cannot test larger blocks than the maximum block size, obviously.
    return;
  }

  string prefix;
  varint::append32(&prefix, input.size());

  // setup compression table
  snappy::internal::workingmemory wmem;
  int table_size;
  uint16* table = wmem.gethashtable(input.size(), &table_size);

  // compress entire input in one shot
  string compressed;
  compressed += prefix;
  compressed.resize(prefix.size()+snappy::maxcompressedlength(input.size()));
  char* dest = string_as_array(&compressed) + prefix.size();
  char* end = snappy::internal::compressfragment(input.data(), input.size(),
                                                dest, table, table_size);
  compressed.resize(end - compressed.data());

  // uncompress into string
  string uncomp_str;
  check(snappy::uncompress(compressed.data(), compressed.size(), &uncomp_str));
  check_eq(uncomp_str, input);

}

// expand the input so that it is at least k times as big as block size
static string expand(const string& input) {
  static const int k = 3;
  string data = input;
  while (data.size() < k * snappy::kblocksize) {
    data += input;
  }
  return data;
}

static int verify(const string& input) {
  vlog(1) << "verifying input of size " << input.size();

  // compress using string based routines
  const int result = verifystring(input);


  verifynonblockedcompression(input);
  verifyiovec(input);
  if (!input.empty()) {
    const string expanded = expand(input);
    verifynonblockedcompression(expanded);
    verifyiovec(input);
  }


  return result;
}

// this test checks to ensure that snappy doesn't coredump if it gets
// corrupted data.

static bool isvalidcompressedbuffer(const string& c) {
  return snappy::isvalidcompressedbuffer(c.data(), c.size());
}
static bool uncompress(const string& c, string* u) {
  return snappy::uncompress(c.data(), c.size(), u);
}

typed_test(corruptedtest, verifycorrupted) {
  string source = "making sure we don't crash with corrupted input";
  vlog(1) << source;
  string dest;
  typeparam uncmp;
  snappy::compress(source.data(), source.size(), &dest);

  // mess around with the data. it's hard to simulate all possible
  // corruptions; this is just one example ...
  check_gt(dest.size(), 3);
  dest[1]--;
  dest[3]++;
  // this really ought to fail.
  check(!isvalidcompressedbuffer(typeparam(dest)));
  check(!uncompress(typeparam(dest), &uncmp));

  // this is testing for a security bug - a buffer that decompresses to 100k
  // but we lie in the snappy header and only reserve 0 bytes of memory :)
  source.resize(100000);
  for (int i = 0; i < source.length(); ++i) {
    source[i] = 'a';
  }
  snappy::compress(source.data(), source.size(), &dest);
  dest[0] = dest[1] = dest[2] = dest[3] = 0;
  check(!isvalidcompressedbuffer(typeparam(dest)));
  check(!uncompress(typeparam(dest), &uncmp));

  if (sizeof(void *) == 4) {
    // another security check; check a crazy big length can't dos us with an
    // over-allocation.
    // currently this is done only for 32-bit builds.  on 64-bit builds,
    // where 3 gb might be an acceptable allocation size, uncompress()
    // attempts to decompress, and sometimes causes the test to run out of
    // memory.
    dest[0] = dest[1] = dest[2] = dest[3] = 0xff;
    // this decodes to a really large size, i.e., about 3 gb.
    dest[4] = 'k';
    check(!isvalidcompressedbuffer(typeparam(dest)));
    check(!uncompress(typeparam(dest), &uncmp));
  } else {
    log(warning) << "crazy decompression lengths not checked on 64-bit build";
  }

  // this decodes to about 2 mb; much smaller, but should still fail.
  dest[0] = dest[1] = dest[2] = 0xff;
  dest[3] = 0x00;
  check(!isvalidcompressedbuffer(typeparam(dest)));
  check(!uncompress(typeparam(dest), &uncmp));

  // try reading stuff in from a bad file.
  for (int i = 1; i <= 3; ++i) {
    string data = readtestdatafile(stringprintf("baddata%d.snappy", i).c_str(),
                                   0);
    string uncmp;
    // check that we don't return a crazy length
    size_t ulen;
    check(!snappy::getuncompressedlength(data.data(), data.size(), &ulen)
          || (ulen < (1<<20)));
    uint32 ulen2;
    snappy::bytearraysource source(data.data(), data.size());
    check(!snappy::getuncompressedlength(&source, &ulen2) ||
          (ulen2 < (1<<20)));
    check(!isvalidcompressedbuffer(typeparam(data)));
    check(!uncompress(typeparam(data), &uncmp));
  }
}

// helper routines to construct arbitrary compressed strings.
// these mirror the compression code in snappy.cc, but are copied
// here so that we can bypass some limitations in the how snappy.cc
// invokes these routines.
static void appendliteral(string* dst, const string& literal) {
  if (literal.empty()) return;
  int n = literal.size() - 1;
  if (n < 60) {
    // fit length in tag byte
    dst->push_back(0 | (n << 2));
  } else {
    // encode in upcoming bytes
    char number[4];
    int count = 0;
    while (n > 0) {
      number[count++] = n & 0xff;
      n >>= 8;
    }
    dst->push_back(0 | ((59+count) << 2));
    *dst += string(number, count);
  }
  *dst += literal;
}

static void appendcopy(string* dst, int offset, int length) {
  while (length > 0) {
    // figure out how much to copy in one shot
    int to_copy;
    if (length >= 68) {
      to_copy = 64;
    } else if (length > 64) {
      to_copy = 60;
    } else {
      to_copy = length;
    }
    length -= to_copy;

    if ((to_copy >= 4) && (to_copy < 12) && (offset < 2048)) {
      assert(to_copy-4 < 8);            // must fit in 3 bits
      dst->push_back(1 | ((to_copy-4) << 2) | ((offset >> 8) << 5));
      dst->push_back(offset & 0xff);
    } else if (offset < 65536) {
      dst->push_back(2 | ((to_copy-1) << 2));
      dst->push_back(offset & 0xff);
      dst->push_back(offset >> 8);
    } else {
      dst->push_back(3 | ((to_copy-1) << 2));
      dst->push_back(offset & 0xff);
      dst->push_back((offset >> 8) & 0xff);
      dst->push_back((offset >> 16) & 0xff);
      dst->push_back((offset >> 24) & 0xff);
    }
  }
}

test(snappy, simpletests) {
  verify("");
  verify("a");
  verify("ab");
  verify("abc");

  verify("aaaaaaa" + string(16, 'b') + string("aaaaa") + "abc");
  verify("aaaaaaa" + string(256, 'b') + string("aaaaa") + "abc");
  verify("aaaaaaa" + string(2047, 'b') + string("aaaaa") + "abc");
  verify("aaaaaaa" + string(65536, 'b') + string("aaaaa") + "abc");
  verify("abcaaaaaaa" + string(65536, 'b') + string("aaaaa") + "abc");
}

// verify max blowup (lots of four-byte copies)
test(snappy, maxblowup) {
  string input;
  for (int i = 0; i < 20000; i++) {
    acmrandom rnd(i);
    uint32 bytes = static_cast<uint32>(rnd.next());
    input.append(reinterpret_cast<char*>(&bytes), sizeof(bytes));
  }
  for (int i = 19999; i >= 0; i--) {
    acmrandom rnd(i);
    uint32 bytes = static_cast<uint32>(rnd.next());
    input.append(reinterpret_cast<char*>(&bytes), sizeof(bytes));
  }
  verify(input);
}

test(snappy, randomdata) {
  acmrandom rnd(flags_test_random_seed);

  const int num_ops = 20000;
  for (int i = 0; i < num_ops; i++) {
    if ((i % 1000) == 0) {
      vlog(0) << "random op " << i << " of " << num_ops;
    }

    string x;
    int len = rnd.uniform(4096);
    if (i < 100) {
      len = 65536 + rnd.uniform(65536);
    }
    while (x.size() < len) {
      int run_len = 1;
      if (rnd.onein(10)) {
        run_len = rnd.skewed(8);
      }
      char c = (i < 100) ? rnd.uniform(256) : rnd.skewed(3);
      while (run_len-- > 0 && x.size() < len) {
        x += c;
      }
    }

    verify(x);
  }
}

test(snappy, fourbyteoffset) {
  // the new compressor cannot generate four-byte offsets since
  // it chops up the input into 32kb pieces.  so we hand-emit the
  // copy manually.

  // the two fragments that make up the input string.
  string fragment1 = "012345689abcdefghijklmnopqrstuvwxyz";
  string fragment2 = "some other string";

  // how many times each fragment is emitted.
  const int n1 = 2;
  const int n2 = 100000 / fragment2.size();
  const int length = n1 * fragment1.size() + n2 * fragment2.size();

  string compressed;
  varint::append32(&compressed, length);

  appendliteral(&compressed, fragment1);
  string src = fragment1;
  for (int i = 0; i < n2; i++) {
    appendliteral(&compressed, fragment2);
    src += fragment2;
  }
  appendcopy(&compressed, src.size(), fragment1.size());
  src += fragment1;
  check_eq(length, src.size());

  string uncompressed;
  check(snappy::isvalidcompressedbuffer(compressed.data(), compressed.size()));
  check(snappy::uncompress(compressed.data(), compressed.size(),
                           &uncompressed));
  check_eq(uncompressed, src);
}

test(snappy, iovecedgecases) {
  // test some tricky edge cases in the iovec output that are not necessarily
  // exercised by random tests.

  // our output blocks look like this initially (the last iovec is bigger
  // than depicted):
  // [  ] [ ] [    ] [        ] [        ]
  static const int klengths[] = { 2, 1, 4, 8, 128 };

  struct iovec iov[arraysize(klengths)];
  for (int i = 0; i < arraysize(klengths); ++i) {
    iov[i].iov_base = new char[klengths[i]];
    iov[i].iov_len = klengths[i];
  }

  string compressed;
  varint::append32(&compressed, 22);

  // a literal whose output crosses three blocks.
  // [ab] [c] [123 ] [        ] [        ]
  appendliteral(&compressed, "abc123");

  // a copy whose output crosses two blocks (source and destination
  // segments marked).
  // [ab] [c] [1231] [23      ] [        ]
  //           ^--^   --
  appendcopy(&compressed, 3, 3);

  // a copy where the input is, at first, in the block before the output:
  //
  // [ab] [c] [1231] [231231  ] [        ]
  //           ^---     ^---
  // then during the copy, the pointers move such that the input and
  // output pointers are in the same block:
  //
  // [ab] [c] [1231] [23123123] [        ]
  //                  ^-    ^-
  // and then they move again, so that the output pointer is no longer
  // in the same block as the input pointer:
  // [ab] [c] [1231] [23123123] [123     ]
  //                    ^--      ^--
  appendcopy(&compressed, 6, 9);

  // finally, a copy where the input is from several blocks back,
  // and it also crosses three blocks:
  //
  // [ab] [c] [1231] [23123123] [123b    ]
  //   ^                            ^
  // [ab] [c] [1231] [23123123] [123bc   ]
  //       ^                         ^
  // [ab] [c] [1231] [23123123] [123bc12 ]
  //           ^-                     ^-
  appendcopy(&compressed, 17, 4);

  check(snappy::rawuncompresstoiovec(
      compressed.data(), compressed.size(), iov, arraysize(iov)));
  check_eq(0, memcmp(iov[0].iov_base, "ab", 2));
  check_eq(0, memcmp(iov[1].iov_base, "c", 1));
  check_eq(0, memcmp(iov[2].iov_base, "1231", 4));
  check_eq(0, memcmp(iov[3].iov_base, "23123123", 8));
  check_eq(0, memcmp(iov[4].iov_base, "123bc12", 7));

  for (int i = 0; i < arraysize(klengths); ++i) {
    delete[] reinterpret_cast<char *>(iov[i].iov_base);
  }
}

test(snappy, iovecliteraloverflow) {
  static const int klengths[] = { 3, 4 };

  struct iovec iov[arraysize(klengths)];
  for (int i = 0; i < arraysize(klengths); ++i) {
    iov[i].iov_base = new char[klengths[i]];
    iov[i].iov_len = klengths[i];
  }

  string compressed;
  varint::append32(&compressed, 8);

  appendliteral(&compressed, "12345678");

  check(!snappy::rawuncompresstoiovec(
      compressed.data(), compressed.size(), iov, arraysize(iov)));

  for (int i = 0; i < arraysize(klengths); ++i) {
    delete[] reinterpret_cast<char *>(iov[i].iov_base);
  }
}

test(snappy, ioveccopyoverflow) {
  static const int klengths[] = { 3, 4 };

  struct iovec iov[arraysize(klengths)];
  for (int i = 0; i < arraysize(klengths); ++i) {
    iov[i].iov_base = new char[klengths[i]];
    iov[i].iov_len = klengths[i];
  }

  string compressed;
  varint::append32(&compressed, 8);

  appendliteral(&compressed, "123");
  appendcopy(&compressed, 3, 5);

  check(!snappy::rawuncompresstoiovec(
      compressed.data(), compressed.size(), iov, arraysize(iov)));

  for (int i = 0; i < arraysize(klengths); ++i) {
    delete[] reinterpret_cast<char *>(iov[i].iov_base);
  }
}


static bool checkuncompressedlength(const string& compressed,
                                    size_t* ulength) {
  const bool result1 = snappy::getuncompressedlength(compressed.data(),
                                                     compressed.size(),
                                                     ulength);

  snappy::bytearraysource source(compressed.data(), compressed.size());
  uint32 length;
  const bool result2 = snappy::getuncompressedlength(&source, &length);
  check_eq(result1, result2);
  return result1;
}

test(snappycorruption, truncatedvarint) {
  string compressed, uncompressed;
  size_t ulength;
  compressed.push_back('\xf0');
  check(!checkuncompressedlength(compressed, &ulength));
  check(!snappy::isvalidcompressedbuffer(compressed.data(), compressed.size()));
  check(!snappy::uncompress(compressed.data(), compressed.size(),
                            &uncompressed));
}

test(snappycorruption, unterminatedvarint) {
  string compressed, uncompressed;
  size_t ulength;
  compressed.push_back(128);
  compressed.push_back(128);
  compressed.push_back(128);
  compressed.push_back(128);
  compressed.push_back(128);
  compressed.push_back(10);
  check(!checkuncompressedlength(compressed, &ulength));
  check(!snappy::isvalidcompressedbuffer(compressed.data(), compressed.size()));
  check(!snappy::uncompress(compressed.data(), compressed.size(),
                            &uncompressed));
}

test(snappy, readpastendofbuffer) {
  // check that we do not read past end of input

  // make a compressed string that ends with a single-byte literal
  string compressed;
  varint::append32(&compressed, 1);
  appendliteral(&compressed, "x");

  string uncompressed;
  dataendingatunreadablepage c(compressed);
  check(snappy::uncompress(c.data(), c.size(), &uncompressed));
  check_eq(uncompressed, string("x"));
}

// check for an infinite loop caused by a copy with offset==0
test(snappy, zerooffsetcopy) {
  const char* compressed = "\x40\x12\x00\x00";
  //  \x40              length (must be > kmaxincrementcopyoverflow)
  //  \x12\x00\x00      copy with offset==0, length==5
  char uncompressed[100];
  expect_false(snappy::rawuncompress(compressed, 4, uncompressed));
}

test(snappy, zerooffsetcopyvalidation) {
  const char* compressed = "\x05\x12\x00\x00";
  //  \x05              length
  //  \x12\x00\x00      copy with offset==0, length==5
  expect_false(snappy::isvalidcompressedbuffer(compressed, 4));
}


namespace {

int testfindmatchlength(const char* s1, const char *s2, unsigned length) {
  return snappy::internal::findmatchlength(s1, s2, s2 + length);
}

}  // namespace

test(snappy, findmatchlength) {
  // exercise all different code paths through the function.
  // 64-bit version:

  // hit s1_limit in 64-bit loop, hit s1_limit in single-character loop.
  expect_eq(6, testfindmatchlength("012345", "012345", 6));
  expect_eq(11, testfindmatchlength("01234567abc", "01234567abc", 11));

  // hit s1_limit in 64-bit loop, find a non-match in single-character loop.
  expect_eq(9, testfindmatchlength("01234567abc", "01234567axc", 9));

  // same, but edge cases.
  expect_eq(11, testfindmatchlength("01234567abc!", "01234567abc!", 11));
  expect_eq(11, testfindmatchlength("01234567abc!", "01234567abc?", 11));

  // find non-match at once in first loop.
  expect_eq(0, testfindmatchlength("01234567xxxxxxxx", "?1234567xxxxxxxx", 16));
  expect_eq(1, testfindmatchlength("01234567xxxxxxxx", "0?234567xxxxxxxx", 16));
  expect_eq(4, testfindmatchlength("01234567xxxxxxxx", "01237654xxxxxxxx", 16));
  expect_eq(7, testfindmatchlength("01234567xxxxxxxx", "0123456?xxxxxxxx", 16));

  // find non-match in first loop after one block.
  expect_eq(8, testfindmatchlength("abcdefgh01234567xxxxxxxx",
                                   "abcdefgh?1234567xxxxxxxx", 24));
  expect_eq(9, testfindmatchlength("abcdefgh01234567xxxxxxxx",
                                   "abcdefgh0?234567xxxxxxxx", 24));
  expect_eq(12, testfindmatchlength("abcdefgh01234567xxxxxxxx",
                                    "abcdefgh01237654xxxxxxxx", 24));
  expect_eq(15, testfindmatchlength("abcdefgh01234567xxxxxxxx",
                                    "abcdefgh0123456?xxxxxxxx", 24));

  // 32-bit version:

  // short matches.
  expect_eq(0, testfindmatchlength("01234567", "?1234567", 8));
  expect_eq(1, testfindmatchlength("01234567", "0?234567", 8));
  expect_eq(2, testfindmatchlength("01234567", "01?34567", 8));
  expect_eq(3, testfindmatchlength("01234567", "012?4567", 8));
  expect_eq(4, testfindmatchlength("01234567", "0123?567", 8));
  expect_eq(5, testfindmatchlength("01234567", "01234?67", 8));
  expect_eq(6, testfindmatchlength("01234567", "012345?7", 8));
  expect_eq(7, testfindmatchlength("01234567", "0123456?", 8));
  expect_eq(7, testfindmatchlength("01234567", "0123456?", 7));
  expect_eq(7, testfindmatchlength("01234567!", "0123456??", 7));

  // hit s1_limit in 32-bit loop, hit s1_limit in single-character loop.
  expect_eq(10, testfindmatchlength("xxxxxxabcd", "xxxxxxabcd", 10));
  expect_eq(10, testfindmatchlength("xxxxxxabcd?", "xxxxxxabcd?", 10));
  expect_eq(13, testfindmatchlength("xxxxxxabcdef", "xxxxxxabcdef", 13));

  // same, but edge cases.
  expect_eq(12, testfindmatchlength("xxxxxx0123abc!", "xxxxxx0123abc!", 12));
  expect_eq(12, testfindmatchlength("xxxxxx0123abc!", "xxxxxx0123abc?", 12));

  // hit s1_limit in 32-bit loop, find a non-match in single-character loop.
  expect_eq(11, testfindmatchlength("xxxxxx0123abc", "xxxxxx0123axc", 13));

  // find non-match at once in first loop.
  expect_eq(6, testfindmatchlength("xxxxxx0123xxxxxxxx",
                                   "xxxxxx?123xxxxxxxx", 18));
  expect_eq(7, testfindmatchlength("xxxxxx0123xxxxxxxx",
                                   "xxxxxx0?23xxxxxxxx", 18));
  expect_eq(8, testfindmatchlength("xxxxxx0123xxxxxxxx",
                                   "xxxxxx0132xxxxxxxx", 18));
  expect_eq(9, testfindmatchlength("xxxxxx0123xxxxxxxx",
                                   "xxxxxx012?xxxxxxxx", 18));

  // same, but edge cases.
  expect_eq(6, testfindmatchlength("xxxxxx0123", "xxxxxx?123", 10));
  expect_eq(7, testfindmatchlength("xxxxxx0123", "xxxxxx0?23", 10));
  expect_eq(8, testfindmatchlength("xxxxxx0123", "xxxxxx0132", 10));
  expect_eq(9, testfindmatchlength("xxxxxx0123", "xxxxxx012?", 10));

  // find non-match in first loop after one block.
  expect_eq(10, testfindmatchlength("xxxxxxabcd0123xx",
                                    "xxxxxxabcd?123xx", 16));
  expect_eq(11, testfindmatchlength("xxxxxxabcd0123xx",
                                    "xxxxxxabcd0?23xx", 16));
  expect_eq(12, testfindmatchlength("xxxxxxabcd0123xx",
                                    "xxxxxxabcd0132xx", 16));
  expect_eq(13, testfindmatchlength("xxxxxxabcd0123xx",
                                    "xxxxxxabcd012?xx", 16));

  // same, but edge cases.
  expect_eq(10, testfindmatchlength("xxxxxxabcd0123", "xxxxxxabcd?123", 14));
  expect_eq(11, testfindmatchlength("xxxxxxabcd0123", "xxxxxxabcd0?23", 14));
  expect_eq(12, testfindmatchlength("xxxxxxabcd0123", "xxxxxxabcd0132", 14));
  expect_eq(13, testfindmatchlength("xxxxxxabcd0123", "xxxxxxabcd012?", 14));
}

test(snappy, findmatchlengthrandom) {
  const int knumtrials = 10000;
  const int ktypicallength = 10;
  acmrandom rnd(flags_test_random_seed);

  for (int i = 0; i < knumtrials; i++) {
    string s, t;
    char a = rnd.rand8();
    char b = rnd.rand8();
    while (!rnd.onein(ktypicallength)) {
      s.push_back(rnd.onein(2) ? a : b);
      t.push_back(rnd.onein(2) ? a : b);
    }
    dataendingatunreadablepage u(s);
    dataendingatunreadablepage v(t);
    int matched = snappy::internal::findmatchlength(
        u.data(), v.data(), v.data() + t.size());
    if (matched == t.size()) {
      expect_eq(s, t);
    } else {
      expect_ne(s[matched], t[matched]);
      for (int j = 0; j < matched; j++) {
        expect_eq(s[j], t[j]);
      }
    }
  }
}


static void compressfile(const char* fname) {
  string fullinput;
  file::getcontents(fname, &fullinput, file::defaults()).checksuccess();

  string compressed;
  compress(fullinput.data(), fullinput.size(), snappy, &compressed, false);

  file::setcontents(string(fname).append(".comp"), compressed, file::defaults())
      .checksuccess();
}

static void uncompressfile(const char* fname) {
  string fullinput;
  file::getcontents(fname, &fullinput, file::defaults()).checksuccess();

  size_t uncomplength;
  check(checkuncompressedlength(fullinput, &uncomplength));

  string uncompressed;
  uncompressed.resize(uncomplength);
  check(snappy::uncompress(fullinput.data(), fullinput.size(), &uncompressed));

  file::setcontents(string(fname).append(".uncomp"), uncompressed,
                    file::defaults()).checksuccess();
}

static void measurefile(const char* fname) {
  string fullinput;
  file::getcontents(fname, &fullinput, file::defaults()).checksuccess();
  printf("%-40s :\n", fname);

  int start_len = (flags_start_len < 0) ? fullinput.size() : flags_start_len;
  int end_len = fullinput.size();
  if (flags_end_len >= 0) {
    end_len = min<int>(fullinput.size(), flags_end_len);
  }
  for (int len = start_len; len <= end_len; len++) {
    const char* const input = fullinput.data();
    int repeats = (flags_bytes + len) / (len + 1);
    if (flags_zlib)     measure(input, len, zlib, repeats, 1024<<10);
    if (flags_lzo)      measure(input, len, lzo, repeats, 1024<<10);
    if (flags_liblzf)   measure(input, len, liblzf, repeats, 1024<<10);
    if (flags_quicklz)  measure(input, len, quicklz, repeats, 1024<<10);
    if (flags_fastlz)   measure(input, len, fastlz, repeats, 1024<<10);
    if (flags_snappy)    measure(input, len, snappy, repeats, 4096<<10);

    // for block-size based measurements
    if (0 && flags_snappy) {
      measure(input, len, snappy, repeats, 8<<10);
      measure(input, len, snappy, repeats, 16<<10);
      measure(input, len, snappy, repeats, 32<<10);
      measure(input, len, snappy, repeats, 64<<10);
      measure(input, len, snappy, repeats, 256<<10);
      measure(input, len, snappy, repeats, 1024<<10);
    }
  }
}

static struct {
  const char* label;
  const char* filename;
  size_t size_limit;
} files[] = {
  { "html", "html", 0 },
  { "urls", "urls.10k", 0 },
  { "jpg", "fireworks.jpeg", 0 },
  { "jpg_200", "fireworks.jpeg", 200 },
  { "pdf", "paper-100k.pdf", 0 },
  { "html4", "html_x_4", 0 },
  { "txt1", "alice29.txt", 0 },
  { "txt2", "asyoulik.txt", 0 },
  { "txt3", "lcet10.txt", 0 },
  { "txt4", "plrabn12.txt", 0 },
  { "pb", "geo.protodata", 0 },
  { "gaviota", "kppkn.gtb", 0 },
};

static void bm_uflat(int iters, int arg) {
  stopbenchmarktiming();

  // pick file to process based on "arg"
  check_ge(arg, 0);
  check_lt(arg, arraysize(files));
  string contents = readtestdatafile(files[arg].filename,
                                     files[arg].size_limit);

  string zcontents;
  snappy::compress(contents.data(), contents.size(), &zcontents);
  char* dst = new char[contents.size()];

  setbenchmarkbytesprocessed(static_cast<int64>(iters) *
                             static_cast<int64>(contents.size()));
  setbenchmarklabel(files[arg].label);
  startbenchmarktiming();
  while (iters-- > 0) {
    check(snappy::rawuncompress(zcontents.data(), zcontents.size(), dst));
  }
  stopbenchmarktiming();

  delete[] dst;
}
benchmark(bm_uflat)->denserange(0, arraysize(files) - 1);

static void bm_uvalidate(int iters, int arg) {
  stopbenchmarktiming();

  // pick file to process based on "arg"
  check_ge(arg, 0);
  check_lt(arg, arraysize(files));
  string contents = readtestdatafile(files[arg].filename,
                                     files[arg].size_limit);

  string zcontents;
  snappy::compress(contents.data(), contents.size(), &zcontents);

  setbenchmarkbytesprocessed(static_cast<int64>(iters) *
                             static_cast<int64>(contents.size()));
  setbenchmarklabel(files[arg].label);
  startbenchmarktiming();
  while (iters-- > 0) {
    check(snappy::isvalidcompressedbuffer(zcontents.data(), zcontents.size()));
  }
  stopbenchmarktiming();
}
benchmark(bm_uvalidate)->denserange(0, 4);

static void bm_uiovec(int iters, int arg) {
  stopbenchmarktiming();

  // pick file to process based on "arg"
  check_ge(arg, 0);
  check_lt(arg, arraysize(files));
  string contents = readtestdatafile(files[arg].filename,
                                     files[arg].size_limit);

  string zcontents;
  snappy::compress(contents.data(), contents.size(), &zcontents);

  // uncompress into an iovec containing ten entries.
  const int knumentries = 10;
  struct iovec iov[knumentries];
  char *dst = new char[contents.size()];
  int used_so_far = 0;
  for (int i = 0; i < knumentries; ++i) {
    iov[i].iov_base = dst + used_so_far;
    if (used_so_far == contents.size()) {
      iov[i].iov_len = 0;
      continue;
    }

    if (i == knumentries - 1) {
      iov[i].iov_len = contents.size() - used_so_far;
    } else {
      iov[i].iov_len = contents.size() / knumentries;
    }
    used_so_far += iov[i].iov_len;
  }

  setbenchmarkbytesprocessed(static_cast<int64>(iters) *
                             static_cast<int64>(contents.size()));
  setbenchmarklabel(files[arg].label);
  startbenchmarktiming();
  while (iters-- > 0) {
    check(snappy::rawuncompresstoiovec(zcontents.data(), zcontents.size(), iov,
                                       knumentries));
  }
  stopbenchmarktiming();

  delete[] dst;
}
benchmark(bm_uiovec)->denserange(0, 4);


static void bm_zflat(int iters, int arg) {
  stopbenchmarktiming();

  // pick file to process based on "arg"
  check_ge(arg, 0);
  check_lt(arg, arraysize(files));
  string contents = readtestdatafile(files[arg].filename,
                                     files[arg].size_limit);

  char* dst = new char[snappy::maxcompressedlength(contents.size())];

  setbenchmarkbytesprocessed(static_cast<int64>(iters) *
                             static_cast<int64>(contents.size()));
  startbenchmarktiming();

  size_t zsize = 0;
  while (iters-- > 0) {
    snappy::rawcompress(contents.data(), contents.size(), dst, &zsize);
  }
  stopbenchmarktiming();
  const double compression_ratio =
      static_cast<double>(zsize) / std::max<size_t>(1, contents.size());
  setbenchmarklabel(stringprintf("%s (%.2f %%)",
                                 files[arg].label, 100.0 * compression_ratio));
  vlog(0) << stringprintf("compression for %s: %zd -> %zd bytes",
                          files[arg].label, contents.size(), zsize);
  delete[] dst;
}
benchmark(bm_zflat)->denserange(0, arraysize(files) - 1);


}  // namespace snappy


int main(int argc, char** argv) {
  initgoogle(argv[0], &argc, &argv, true);
  runspecifiedbenchmarks();


  if (argc >= 2) {
    for (int arg = 1; arg < argc; arg++) {
      if (flags_write_compressed) {
        compressfile(argv[arg]);
      } else if (flags_write_uncompressed) {
        uncompressfile(argv[arg]);
      } else {
        measurefile(argv[arg]);
      }
    }
    return 0;
  }

  return run_all_tests();
}
