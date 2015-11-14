// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
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

// from google3/strings/strutil.h

#ifndef google_protobuf_stubs_strutil_h__
#define google_protobuf_stubs_strutil_h__

#include <stdlib.h>
#include <vector>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

#ifdef _msc_ver
#define strtoll  _strtoi64
#define strtoull _strtoui64
#elif defined(__deccxx) && defined(__osf__)
// hp c++ on tru64 does not have strtoll, but strtol is already 64-bit.
#define strtoll strtol
#define strtoull strtoul
#endif

// ----------------------------------------------------------------------
// ascii_isalnum()
//    check if an ascii character is alphanumeric.  we can't use ctype's
//    isalnum() because it is affected by locale.  this function is applied
//    to identifiers in the protocol buffer language, not to natural-language
//    strings, so locale should not be taken into account.
// ascii_isdigit()
//    like above, but only accepts digits.
// ----------------------------------------------------------------------

inline bool ascii_isalnum(char c) {
  return ('a' <= c && c <= 'z') ||
         ('a' <= c && c <= 'z') ||
         ('0' <= c && c <= '9');
}

inline bool ascii_isdigit(char c) {
  return ('0' <= c && c <= '9');
}

// ----------------------------------------------------------------------
// hasprefixstring()
//    check if a string begins with a given prefix.
// stripprefixstring()
//    given a string and a putative prefix, returns the string minus the
//    prefix string if the prefix matches, otherwise the original
//    string.
// ----------------------------------------------------------------------
inline bool hasprefixstring(const string& str,
                            const string& prefix) {
  return str.size() >= prefix.size() &&
         str.compare(0, prefix.size(), prefix) == 0;
}

inline string stripprefixstring(const string& str, const string& prefix) {
  if (hasprefixstring(str, prefix)) {
    return str.substr(prefix.size());
  } else {
    return str;
  }
}

// ----------------------------------------------------------------------
// hassuffixstring()
//    return true if str ends in suffix.
// stripsuffixstring()
//    given a string and a putative suffix, returns the string minus the
//    suffix string if the suffix matches, otherwise the original
//    string.
// ----------------------------------------------------------------------
inline bool hassuffixstring(const string& str,
                            const string& suffix) {
  return str.size() >= suffix.size() &&
         str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline string stripsuffixstring(const string& str, const string& suffix) {
  if (hassuffixstring(str, suffix)) {
    return str.substr(0, str.size() - suffix.size());
  } else {
    return str;
  }
}

// ----------------------------------------------------------------------
// stripstring
//    replaces any occurrence of the character 'remove' (or the characters
//    in 'remove') with the character 'replacewith'.
//    good for keeping html characters or protocol characters (\t) out
//    of places where they might cause a problem.
// ----------------------------------------------------------------------
libprotobuf_export void stripstring(string* s, const char* remove,
                                    char replacewith);

// ----------------------------------------------------------------------
// lowerstring()
// upperstring()
//    convert the characters in "s" to lowercase or uppercase.  ascii-only:
//    these functions intentionally ignore locale because they are applied to
//    identifiers used in the protocol buffer language, not to natural-language
//    strings.
// ----------------------------------------------------------------------

inline void lowerstring(string * s) {
  string::iterator end = s->end();
  for (string::iterator i = s->begin(); i != end; ++i) {
    // tolower() changes based on locale.  we don't want this!
    if ('a' <= *i && *i <= 'z') *i += 'a' - 'a';
  }
}

inline void upperstring(string * s) {
  string::iterator end = s->end();
  for (string::iterator i = s->begin(); i != end; ++i) {
    // toupper() changes based on locale.  we don't want this!
    if ('a' <= *i && *i <= 'z') *i += 'a' - 'a';
  }
}

// ----------------------------------------------------------------------
// stringreplace()
//    give me a string and two patterns "old" and "new", and i replace
//    the first instance of "old" in the string with "new", if it
//    exists.  return a new string, regardless of whether the replacement
//    happened or not.
// ----------------------------------------------------------------------

libprotobuf_export string stringreplace(const string& s, const string& oldsub,
                                        const string& newsub, bool replace_all);

// ----------------------------------------------------------------------
// splitstringusing()
//    split a string using a character delimiter. append the components
//    to 'result'.  if there are consecutive delimiters, this function skips
//    over all of them.
// ----------------------------------------------------------------------
libprotobuf_export void splitstringusing(const string& full, const char* delim,
                                         vector<string>* res);

// split a string using one or more byte delimiters, presented
// as a nul-terminated c string. append the components to 'result'.
// if there are consecutive delimiters, this function will return
// corresponding empty strings.  if you want to drop the empty
// strings, try splitstringusing().
//
// if "full" is the empty string, yields an empty string as the only value.
// ----------------------------------------------------------------------
libprotobuf_export void splitstringallowempty(const string& full,
                                              const char* delim,
                                              vector<string>* result);

// ----------------------------------------------------------------------
// joinstrings()
//    these methods concatenate a vector of strings into a c++ string, using
//    the c-string "delim" as a separator between components. there are two
//    flavors of the function, one flavor returns the concatenated string,
//    another takes a pointer to the target string. in the latter case the
//    target string is cleared and overwritten.
// ----------------------------------------------------------------------
libprotobuf_export void joinstrings(const vector<string>& components,
                                    const char* delim, string* result);

inline string joinstrings(const vector<string>& components,
                          const char* delim) {
  string result;
  joinstrings(components, delim, &result);
  return result;
}

// ----------------------------------------------------------------------
// unescapecescapesequences()
//    copies "source" to "dest", rewriting c-style escape sequences
//    -- '\n', '\r', '\\', '\ooo', etc -- to their ascii
//    equivalents.  "dest" must be sufficiently large to hold all
//    the characters in the rewritten string (i.e. at least as large
//    as strlen(source) + 1 should be safe, since the replacements
//    are always shorter than the original escaped sequences).  it's
//    safe for source and dest to be the same.  returns the length
//    of dest.
//
//    it allows hex sequences \xhh, or generally \xhhhhh with an
//    arbitrary number of hex digits, but all of them together must
//    specify a value of a single byte (e.g. \x0045 is equivalent
//    to \x45, and \x1234 is erroneous).
//
//    it also allows escape sequences of the form \uhhhh (exactly four
//    hex digits, upper or lower case) or \uhhhhhhhh (exactly eight
//    hex digits, upper or lower case) to specify a unicode code
//    point. the dest array will contain the utf8-encoded version of
//    that code-point (e.g., if source contains \u2019, then dest will
//    contain the three bytes 0xe2, 0x80, and 0x99).
//
//    errors: in the first form of the call, errors are reported with
//    log(error). the same is true for the second form of the call if
//    the pointer to the string vector is null; otherwise, error
//    messages are stored in the vector. in either case, the effect on
//    the dest array is not defined, but rest of the source will be
//    processed.
//    ----------------------------------------------------------------------

libprotobuf_export int unescapecescapesequences(const char* source, char* dest);
libprotobuf_export int unescapecescapesequences(const char* source, char* dest,
                                                vector<string> *errors);

// ----------------------------------------------------------------------
// unescapecescapestring()
//    this does the same thing as unescapecescapesequences, but creates
//    a new string. the caller does not need to worry about allocating
//    a dest buffer. this should be used for non performance critical
//    tasks such as printing debug messages. it is safe for src and dest
//    to be the same.
//
//    the second call stores its errors in a supplied string vector.
//    if the string vector pointer is null, it reports the errors with log().
//
//    in the first and second calls, the length of dest is returned. in the
//    the third call, the new string is returned.
// ----------------------------------------------------------------------

libprotobuf_export int unescapecescapestring(const string& src, string* dest);
libprotobuf_export int unescapecescapestring(const string& src, string* dest,
                                             vector<string> *errors);
libprotobuf_export string unescapecescapestring(const string& src);

// ----------------------------------------------------------------------
// cescapestring()
//    copies 'src' to 'dest', escaping dangerous characters using
//    c-style escape sequences. this is very useful for preparing query
//    flags. 'src' and 'dest' should not overlap.
//    returns the number of bytes written to 'dest' (not including the \0)
//    or -1 if there was insufficient space.
//
//    currently only \n, \r, \t, ", ', \ and !isprint() chars are escaped.
// ----------------------------------------------------------------------
libprotobuf_export int cescapestring(const char* src, int src_len,
                                     char* dest, int dest_len);

// ----------------------------------------------------------------------
// cescape()
//    more convenient form of cescapestring: returns result as a "string".
//    this version is slower than cescapestring() because it does more
//    allocation.  however, it is much more convenient to use in
//    non-speed-critical code like logging messages etc.
// ----------------------------------------------------------------------
libprotobuf_export string cescape(const string& src);

namespace strings {
// like cescape() but does not escape bytes with the upper bit set.
libprotobuf_export string utf8safecescape(const string& src);

// like cescape() but uses hex (\x) escapes instead of octals.
libprotobuf_export string chexescape(const string& src);
}  // namespace strings

// ----------------------------------------------------------------------
// strto32()
// strtou32()
// strto64()
// strtou64()
//    architecture-neutral plug compatible replacements for strtol() and
//    strtoul().  long's have different lengths on ilp-32 and lp-64
//    platforms, so using these is safer, from the point of view of
//    overflow behavior, than using the standard libc functions.
// ----------------------------------------------------------------------
libprotobuf_export int32 strto32_adaptor(const char *nptr, char **endptr,
                                         int base);
libprotobuf_export uint32 strtou32_adaptor(const char *nptr, char **endptr,
                                           int base);

inline int32 strto32(const char *nptr, char **endptr, int base) {
  if (sizeof(int32) == sizeof(long))
    return strtol(nptr, endptr, base);
  else
    return strto32_adaptor(nptr, endptr, base);
}

inline uint32 strtou32(const char *nptr, char **endptr, int base) {
  if (sizeof(uint32) == sizeof(unsigned long))
    return strtoul(nptr, endptr, base);
  else
    return strtou32_adaptor(nptr, endptr, base);
}

// for now, long long is 64-bit on all the platforms we care about, so these
// functions can simply pass the call to strto[u]ll.
inline int64 strto64(const char *nptr, char **endptr, int base) {
  google_compile_assert(sizeof(int64) == sizeof(long long),
                        sizeof_int64_is_not_sizeof_long_long);
  return strtoll(nptr, endptr, base);
}

inline uint64 strtou64(const char *nptr, char **endptr, int base) {
  google_compile_assert(sizeof(uint64) == sizeof(unsigned long long),
                        sizeof_uint64_is_not_sizeof_long_long);
  return strtoull(nptr, endptr, base);
}

// ----------------------------------------------------------------------
// fastinttobuffer()
// fasthextobuffer()
// fasthex64tobuffer()
// fasthex32tobuffer()
// fasttimetobuffer()
//    these are intended for speed.  fastinttobuffer() assumes the
//    integer is non-negative.  fasthextobuffer() puts output in
//    hex rather than decimal.  fasttimetobuffer() puts the output
//    into rfc822 format.
//
//    fasthex64tobuffer() puts a 64-bit unsigned value in hex-format,
//    padded to exactly 16 bytes (plus one byte for '\0')
//
//    fasthex32tobuffer() puts a 32-bit unsigned value in hex-format,
//    padded to exactly 8 bytes (plus one byte for '\0')
//
//       all functions take the output buffer as an arg.
//    they all return a pointer to the beginning of the output,
//    which may not be the beginning of the input buffer.
// ----------------------------------------------------------------------

// suggested buffer size for fasttobuffer functions.  also works with
// doubletobuffer() and floattobuffer().
static const int kfasttobuffersize = 32;

libprotobuf_export char* fastint32tobuffer(int32 i, char* buffer);
libprotobuf_export char* fastint64tobuffer(int64 i, char* buffer);
char* fastuint32tobuffer(uint32 i, char* buffer);  // inline below
char* fastuint64tobuffer(uint64 i, char* buffer);  // inline below
libprotobuf_export char* fasthextobuffer(int i, char* buffer);
libprotobuf_export char* fasthex64tobuffer(uint64 i, char* buffer);
libprotobuf_export char* fasthex32tobuffer(uint32 i, char* buffer);

// at least 22 bytes long
inline char* fastinttobuffer(int i, char* buffer) {
  return (sizeof(i) == 4 ?
          fastint32tobuffer(i, buffer) : fastint64tobuffer(i, buffer));
}
inline char* fastuinttobuffer(unsigned int i, char* buffer) {
  return (sizeof(i) == 4 ?
          fastuint32tobuffer(i, buffer) : fastuint64tobuffer(i, buffer));
}
inline char* fastlongtobuffer(long i, char* buffer) {
  return (sizeof(i) == 4 ?
          fastint32tobuffer(i, buffer) : fastint64tobuffer(i, buffer));
}
inline char* fastulongtobuffer(unsigned long i, char* buffer) {
  return (sizeof(i) == 4 ?
          fastuint32tobuffer(i, buffer) : fastuint64tobuffer(i, buffer));
}

// ----------------------------------------------------------------------
// fastint32tobufferleft()
// fastuint32tobufferleft()
// fastint64tobufferleft()
// fastuint64tobufferleft()
//
// like the fast*tobuffer() functions above, these are intended for speed.
// unlike the fast*tobuffer() functions, however, these functions write
// their output to the beginning of the buffer (hence the name, as the
// output is left-aligned).  the caller is responsible for ensuring that
// the buffer has enough space to hold the output.
//
// returns a pointer to the end of the string (i.e. the null character
// terminating the string).
// ----------------------------------------------------------------------

libprotobuf_export char* fastint32tobufferleft(int32 i, char* buffer);
libprotobuf_export char* fastuint32tobufferleft(uint32 i, char* buffer);
libprotobuf_export char* fastint64tobufferleft(int64 i, char* buffer);
libprotobuf_export char* fastuint64tobufferleft(uint64 i, char* buffer);

// just define these in terms of the above.
inline char* fastuint32tobuffer(uint32 i, char* buffer) {
  fastuint32tobufferleft(i, buffer);
  return buffer;
}
inline char* fastuint64tobuffer(uint64 i, char* buffer) {
  fastuint64tobufferleft(i, buffer);
  return buffer;
}

// ----------------------------------------------------------------------
// simpleitoa()
//    description: converts an integer to a string.
//
//    return value: string
// ----------------------------------------------------------------------
libprotobuf_export string simpleitoa(int i);
libprotobuf_export string simpleitoa(unsigned int i);
libprotobuf_export string simpleitoa(long i);
libprotobuf_export string simpleitoa(unsigned long i);
libprotobuf_export string simpleitoa(long long i);
libprotobuf_export string simpleitoa(unsigned long long i);

// ----------------------------------------------------------------------
// simpledtoa()
// simpleftoa()
// doubletobuffer()
// floattobuffer()
//    description: converts a double or float to a string which, if
//    passed to nolocalestrtod(), will produce the exact same original double
//    (except in case of nan; all nans are considered the same value).
//    we try to keep the string short but it's not guaranteed to be as
//    short as possible.
//
//    doubletobuffer() and floattobuffer() write the text to the given
//    buffer and return it.  the buffer must be at least
//    kdoubletobuffersize bytes for doubles and kfloattobuffersize
//    bytes for floats.  kfasttobuffersize is also guaranteed to be large
//    enough to hold either.
//
//    return value: string
// ----------------------------------------------------------------------
libprotobuf_export string simpledtoa(double value);
libprotobuf_export string simpleftoa(float value);

libprotobuf_export char* doubletobuffer(double i, char* buffer);
libprotobuf_export char* floattobuffer(float i, char* buffer);

// in practice, doubles should never need more than 24 bytes and floats
// should never need more than 14 (including null terminators), but we
// overestimate to be safe.
static const int kdoubletobuffersize = 32;
static const int kfloattobuffersize = 24;

// ----------------------------------------------------------------------
// nolocalestrtod()
//   exactly like strtod(), except it always behaves as if in the "c"
//   locale (i.e. decimal points must be '.'s).
// ----------------------------------------------------------------------

libprotobuf_export double nolocalestrtod(const char* text, char** endptr);

}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_stubs_strutil_h__
