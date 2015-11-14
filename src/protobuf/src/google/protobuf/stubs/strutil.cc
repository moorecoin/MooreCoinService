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

// from google3/strings/strutil.cc

#include <google/protobuf/stubs/strutil.h>
#include <errno.h>
#include <float.h>    // flt_dig and dbl_dig
#include <limits>
#include <limits.h>
#include <stdio.h>
#include <iterator>

#ifdef _win32
// msvc has only _snprintf, not snprintf.
//
// mingw has both snprintf and _snprintf, but they appear to be different
// functions.  the former is buggy.  when invoked like so:
//   char buffer[32];
//   snprintf(buffer, 32, "%.*g\n", flt_dig, 1.23e10f);
// it prints "1.23000e+10".  this is plainly wrong:  %g should never print
// trailing zeros after the decimal point.  for some reason this bug only
// occurs with some input values, not all.  in any case, _snprintf does the
// right thing, so we use it.
#define snprintf _snprintf
#endif

namespace google {
namespace protobuf {

inline bool isnan(double value) {
  // nan is never equal to anything, even itself.
  return value != value;
}

// these are defined as macros on some platforms.  #undef them so that we can
// redefine them.
#undef isxdigit
#undef isprint

// the definitions of these in ctype.h change based on locale.  since our
// string manipulation is all in relation to the protocol buffer and c++
// languages, we always want to use the c locale.  so, we re-define these
// exactly as we want them.
inline bool isxdigit(char c) {
  return ('0' <= c && c <= '9') ||
         ('a' <= c && c <= 'f') ||
         ('a' <= c && c <= 'f');
}

inline bool isprint(char c) {
  return c >= 0x20 && c <= 0x7e;
}

// ----------------------------------------------------------------------
// stripstring
//    replaces any occurrence of the character 'remove' (or the characters
//    in 'remove') with the character 'replacewith'.
// ----------------------------------------------------------------------
void stripstring(string* s, const char* remove, char replacewith) {
  const char * str_start = s->c_str();
  const char * str = str_start;
  for (str = strpbrk(str, remove);
       str != null;
       str = strpbrk(str + 1, remove)) {
    (*s)[str - str_start] = replacewith;
  }
}

// ----------------------------------------------------------------------
// stringreplace()
//    replace the "old" pattern with the "new" pattern in a string,
//    and append the result to "res".  if replace_all is false,
//    it only replaces the first instance of "old."
// ----------------------------------------------------------------------

void stringreplace(const string& s, const string& oldsub,
                   const string& newsub, bool replace_all,
                   string* res) {
  if (oldsub.empty()) {
    res->append(s);  // if empty, append the given string.
    return;
  }

  string::size_type start_pos = 0;
  string::size_type pos;
  do {
    pos = s.find(oldsub, start_pos);
    if (pos == string::npos) {
      break;
    }
    res->append(s, start_pos, pos - start_pos);
    res->append(newsub);
    start_pos = pos + oldsub.size();  // start searching again after the "old"
  } while (replace_all);
  res->append(s, start_pos, s.length() - start_pos);
}

// ----------------------------------------------------------------------
// stringreplace()
//    give me a string and two patterns "old" and "new", and i replace
//    the first instance of "old" in the string with "new", if it
//    exists.  if "global" is true; call this repeatedly until it
//    fails.  return a new string, regardless of whether the replacement
//    happened or not.
// ----------------------------------------------------------------------

string stringreplace(const string& s, const string& oldsub,
                     const string& newsub, bool replace_all) {
  string ret;
  stringreplace(s, oldsub, newsub, replace_all, &ret);
  return ret;
}

// ----------------------------------------------------------------------
// splitstringusing()
//    split a string using a character delimiter. append the components
//    to 'result'.
//
// note: for multi-character delimiters, this routine will split on *any* of
// the characters in the string, not the entire string as a single delimiter.
// ----------------------------------------------------------------------
template <typename itr>
static inline
void splitstringtoiteratorusing(const string& full,
                                const char* delim,
                                itr& result) {
  // optimize the common case where delim is a single character.
  if (delim[0] != '\0' && delim[1] == '\0') {
    char c = delim[0];
    const char* p = full.data();
    const char* end = p + full.size();
    while (p != end) {
      if (*p == c) {
        ++p;
      } else {
        const char* start = p;
        while (++p != end && *p != c);
        *result++ = string(start, p - start);
      }
    }
    return;
  }

  string::size_type begin_index, end_index;
  begin_index = full.find_first_not_of(delim);
  while (begin_index != string::npos) {
    end_index = full.find_first_of(delim, begin_index);
    if (end_index == string::npos) {
      *result++ = full.substr(begin_index);
      return;
    }
    *result++ = full.substr(begin_index, (end_index - begin_index));
    begin_index = full.find_first_not_of(delim, end_index);
  }
}

void splitstringusing(const string& full,
                      const char* delim,
                      vector<string>* result) {
  back_insert_iterator< vector<string> > it(*result);
  splitstringtoiteratorusing(full, delim, it);
}

// split a string using a character delimiter. append the components
// to 'result'.  if there are consecutive delimiters, this function
// will return corresponding empty strings. the string is split into
// at most the specified number of pieces greedily. this means that the
// last piece may possibly be split further. to split into as many pieces
// as possible, specify 0 as the number of pieces.
//
// if "full" is the empty string, yields an empty string as the only value.
//
// if "pieces" is negative for some reason, it returns the whole string
// ----------------------------------------------------------------------
template <typename stringtype, typename itr>
static inline
void splitstringtoiteratorallowempty(const stringtype& full,
                                     const char* delim,
                                     int pieces,
                                     itr& result) {
  string::size_type begin_index, end_index;
  begin_index = 0;

  for (int i = 0; (i < pieces-1) || (pieces == 0); i++) {
    end_index = full.find_first_of(delim, begin_index);
    if (end_index == string::npos) {
      *result++ = full.substr(begin_index);
      return;
    }
    *result++ = full.substr(begin_index, (end_index - begin_index));
    begin_index = end_index + 1;
  }
  *result++ = full.substr(begin_index);
}

void splitstringallowempty(const string& full, const char* delim,
                           vector<string>* result) {
  back_insert_iterator<vector<string> > it(*result);
  splitstringtoiteratorallowempty(full, delim, 0, it);
}

// ----------------------------------------------------------------------
// joinstrings()
//    this merges a vector of string components with delim inserted
//    as separaters between components.
//
// ----------------------------------------------------------------------
template <class iterator>
static void joinstringsiterator(const iterator& start,
                                const iterator& end,
                                const char* delim,
                                string* result) {
  google_check(result != null);
  result->clear();
  int delim_length = strlen(delim);

  // precompute resulting length so we can reserve() memory in one shot.
  int length = 0;
  for (iterator iter = start; iter != end; ++iter) {
    if (iter != start) {
      length += delim_length;
    }
    length += iter->size();
  }
  result->reserve(length);

  // now combine everything.
  for (iterator iter = start; iter != end; ++iter) {
    if (iter != start) {
      result->append(delim, delim_length);
    }
    result->append(iter->data(), iter->size());
  }
}

void joinstrings(const vector<string>& components,
                 const char* delim,
                 string * result) {
  joinstringsiterator(components.begin(), components.end(), delim, result);
}

// ----------------------------------------------------------------------
// unescapecescapesequences()
//    this does all the unescaping that c does: \ooo, \r, \n, etc
//    returns length of resulting string.
//    the implementation of \x parses any positive number of hex digits,
//    but it is an error if the value requires more than 8 bits, and the
//    result is truncated to 8 bits.
//
//    the second call stores its errors in a supplied string vector.
//    if the string vector pointer is null, it reports the errors with log().
// ----------------------------------------------------------------------

#define is_octal_digit(c) (((c) >= '0') && ((c) <= '7'))

inline int hex_digit_to_int(char c) {
  /* assume ascii. */
  assert('0' == 0x30 && 'a' == 0x41 && 'a' == 0x61);
  assert(isxdigit(c));
  int x = static_cast<unsigned char>(c);
  if (x > '9') {
    x += 9;
  }
  return x & 0xf;
}

// protocol buffers doesn't ever care about errors, but i don't want to remove
// the code.
#define log_string(level, vector) google_log_if(level, false)

int unescapecescapesequences(const char* source, char* dest) {
  return unescapecescapesequences(source, dest, null);
}

int unescapecescapesequences(const char* source, char* dest,
                             vector<string> *errors) {
  google_dcheck(errors == null) << "error reporting not implemented.";

  char* d = dest;
  const char* p = source;

  // small optimization for case where source = dest and there's no escaping
  while ( p == d && *p != '\0' && *p != '\\' )
    p++, d++;

  while (*p != '\0') {
    if (*p != '\\') {
      *d++ = *p++;
    } else {
      switch ( *++p ) {                    // skip past the '\\'
        case '\0':
          log_string(error, errors) << "string cannot end with \\";
          *d = '\0';
          return d - dest;   // we're done with p
        case 'a':  *d++ = '\a';  break;
        case 'b':  *d++ = '\b';  break;
        case 'f':  *d++ = '\f';  break;
        case 'n':  *d++ = '\n';  break;
        case 'r':  *d++ = '\r';  break;
        case 't':  *d++ = '\t';  break;
        case 'v':  *d++ = '\v';  break;
        case '\\': *d++ = '\\';  break;
        case '?':  *d++ = '\?';  break;    // \?  who knew?
        case '\'': *d++ = '\'';  break;
        case '"':  *d++ = '\"';  break;
        case '0': case '1': case '2': case '3':  // octal digit: 1 to 3 digits
        case '4': case '5': case '6': case '7': {
          char ch = *p - '0';
          if ( is_octal_digit(p[1]) )
            ch = ch * 8 + *++p - '0';
          if ( is_octal_digit(p[1]) )      // safe (and easy) to do this twice
            ch = ch * 8 + *++p - '0';      // now points at last digit
          *d++ = ch;
          break;
        }
        case 'x': case 'x': {
          if (!isxdigit(p[1])) {
            if (p[1] == '\0') {
              log_string(error, errors) << "string cannot end with \\x";
            } else {
              log_string(error, errors) <<
                "\\x cannot be followed by non-hex digit: \\" << *p << p[1];
            }
            break;
          }
          unsigned int ch = 0;
          const char *hex_start = p;
          while (isxdigit(p[1]))  // arbitrarily many hex digits
            ch = (ch << 4) + hex_digit_to_int(*++p);
          if (ch > 0xff)
            log_string(error, errors) << "value of " <<
              "\\" << string(hex_start, p+1-hex_start) << " exceeds 8 bits";
          *d++ = ch;
          break;
        }
#if 0  // todo(kenton):  support \u and \u?  requires runetochar().
        case 'u': {
          // \uhhhh => convert 4 hex digits to utf-8
          char32 rune = 0;
          const char *hex_start = p;
          for (int i = 0; i < 4; ++i) {
            if (isxdigit(p[1])) {  // look one char ahead.
              rune = (rune << 4) + hex_digit_to_int(*++p);  // advance p.
            } else {
              log_string(error, errors)
                << "\\u must be followed by 4 hex digits: \\"
                <<  string(hex_start, p+1-hex_start);
              break;
            }
          }
          d += runetochar(d, &rune);
          break;
        }
        case 'u': {
          // \uhhhhhhhh => convert 8 hex digits to utf-8
          char32 rune = 0;
          const char *hex_start = p;
          for (int i = 0; i < 8; ++i) {
            if (isxdigit(p[1])) {  // look one char ahead.
              // don't change rune until we're sure this
              // is within the unicode limit, but do advance p.
              char32 newrune = (rune << 4) + hex_digit_to_int(*++p);
              if (newrune > 0x10ffff) {
                log_string(error, errors)
                  << "value of \\"
                  << string(hex_start, p + 1 - hex_start)
                  << " exceeds unicode limit (0x10ffff)";
                break;
              } else {
                rune = newrune;
              }
            } else {
              log_string(error, errors)
                << "\\u must be followed by 8 hex digits: \\"
                <<  string(hex_start, p+1-hex_start);
              break;
            }
          }
          d += runetochar(d, &rune);
          break;
        }
#endif
        default:
          log_string(error, errors) << "unknown escape sequence: \\" << *p;
      }
      p++;                                 // read past letter we escaped
    }
  }
  *d = '\0';
  return d - dest;
}

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
int unescapecescapestring(const string& src, string* dest) {
  return unescapecescapestring(src, dest, null);
}

int unescapecescapestring(const string& src, string* dest,
                          vector<string> *errors) {
  scoped_array<char> unescaped(new char[src.size() + 1]);
  int len = unescapecescapesequences(src.c_str(), unescaped.get(), errors);
  google_check(dest);
  dest->assign(unescaped.get(), len);
  return len;
}

string unescapecescapestring(const string& src) {
  scoped_array<char> unescaped(new char[src.size() + 1]);
  int len = unescapecescapesequences(src.c_str(), unescaped.get(), null);
  return string(unescaped.get(), len);
}

// ----------------------------------------------------------------------
// cescapestring()
// chexescapestring()
//    copies 'src' to 'dest', escaping dangerous characters using
//    c-style escape sequences. this is very useful for preparing query
//    flags. 'src' and 'dest' should not overlap. the 'hex' version uses
//    hexadecimal rather than octal sequences.
//    returns the number of bytes written to 'dest' (not including the \0)
//    or -1 if there was insufficient space.
//
//    currently only \n, \r, \t, ", ', \ and !isprint() chars are escaped.
// ----------------------------------------------------------------------
int cescapeinternal(const char* src, int src_len, char* dest,
                    int dest_len, bool use_hex, bool utf8_safe) {
  const char* src_end = src + src_len;
  int used = 0;
  bool last_hex_escape = false; // true if last output char was \xnn

  for (; src < src_end; src++) {
    if (dest_len - used < 2)   // need space for two letter escape
      return -1;

    bool is_hex_escape = false;
    switch (*src) {
      case '\n': dest[used++] = '\\'; dest[used++] = 'n';  break;
      case '\r': dest[used++] = '\\'; dest[used++] = 'r';  break;
      case '\t': dest[used++] = '\\'; dest[used++] = 't';  break;
      case '\"': dest[used++] = '\\'; dest[used++] = '\"'; break;
      case '\'': dest[used++] = '\\'; dest[used++] = '\''; break;
      case '\\': dest[used++] = '\\'; dest[used++] = '\\'; break;
      default:
        // note that if we emit \xnn and the src character after that is a hex
        // digit then that digit must be escaped too to prevent it being
        // interpreted as part of the character code by c.
        if ((!utf8_safe || static_cast<uint8>(*src) < 0x80) &&
            (!isprint(*src) ||
             (last_hex_escape && isxdigit(*src)))) {
          if (dest_len - used < 4) // need space for 4 letter escape
            return -1;
          sprintf(dest + used, (use_hex ? "\\x%02x" : "\\%03o"),
                  static_cast<uint8>(*src));
          is_hex_escape = use_hex;
          used += 4;
        } else {
          dest[used++] = *src; break;
        }
    }
    last_hex_escape = is_hex_escape;
  }

  if (dest_len - used < 1)   // make sure that there is room for \0
    return -1;

  dest[used] = '\0';   // doesn't count towards return value though
  return used;
}

int cescapestring(const char* src, int src_len, char* dest, int dest_len) {
  return cescapeinternal(src, src_len, dest, dest_len, false, false);
}

// ----------------------------------------------------------------------
// cescape()
// chexescape()
//    copies 'src' to result, escaping dangerous characters using
//    c-style escape sequences. this is very useful for preparing query
//    flags. 'src' and 'dest' should not overlap. the 'hex' version
//    hexadecimal rather than octal sequences.
//
//    currently only \n, \r, \t, ", ', \ and !isprint() chars are escaped.
// ----------------------------------------------------------------------
string cescape(const string& src) {
  const int dest_length = src.size() * 4 + 1; // maximum possible expansion
  scoped_array<char> dest(new char[dest_length]);
  const int len = cescapeinternal(src.data(), src.size(),
                                  dest.get(), dest_length, false, false);
  google_dcheck_ge(len, 0);
  return string(dest.get(), len);
}

namespace strings {

string utf8safecescape(const string& src) {
  const int dest_length = src.size() * 4 + 1; // maximum possible expansion
  scoped_array<char> dest(new char[dest_length]);
  const int len = cescapeinternal(src.data(), src.size(),
                                  dest.get(), dest_length, false, true);
  google_dcheck_ge(len, 0);
  return string(dest.get(), len);
}

string chexescape(const string& src) {
  const int dest_length = src.size() * 4 + 1; // maximum possible expansion
  scoped_array<char> dest(new char[dest_length]);
  const int len = cescapeinternal(src.data(), src.size(),
                                  dest.get(), dest_length, true, false);
  google_dcheck_ge(len, 0);
  return string(dest.get(), len);
}

}  // namespace strings

// ----------------------------------------------------------------------
// strto32_adaptor()
// strtou32_adaptor()
//    implementation of strto[u]l replacements that have identical
//    overflow and underflow characteristics for both ilp-32 and lp-64
//    platforms, including errno preservation in error-free calls.
// ----------------------------------------------------------------------

int32 strto32_adaptor(const char *nptr, char **endptr, int base) {
  const int saved_errno = errno;
  errno = 0;
  const long result = strtol(nptr, endptr, base);
  if (errno == erange && result == long_min) {
    return kint32min;
  } else if (errno == erange && result == long_max) {
    return kint32max;
  } else if (errno == 0 && result < kint32min) {
    errno = erange;
    return kint32min;
  } else if (errno == 0 && result > kint32max) {
    errno = erange;
    return kint32max;
  }
  if (errno == 0)
    errno = saved_errno;
  return static_cast<int32>(result);
}

uint32 strtou32_adaptor(const char *nptr, char **endptr, int base) {
  const int saved_errno = errno;
  errno = 0;
  const unsigned long result = strtoul(nptr, endptr, base);
  if (errno == erange && result == ulong_max) {
    return kuint32max;
  } else if (errno == 0 && result > kuint32max) {
    errno = erange;
    return kuint32max;
  }
  if (errno == 0)
    errno = saved_errno;
  return static_cast<uint32>(result);
}

// ----------------------------------------------------------------------
// fastinttobuffer()
// fastint64tobuffer()
// fasthextobuffer()
// fasthex64tobuffer()
// fasthex32tobuffer()
// ----------------------------------------------------------------------

// offset into buffer where fastint64tobuffer places the end of string
// null character.  also used by fastint64tobufferleft.
static const int kfastint64tobufferoffset = 21;

char *fastint64tobuffer(int64 i, char* buffer) {
  // we could collapse the positive and negative sections, but that
  // would be slightly slower for positive numbers...
  // 22 bytes is enough to store -2**64, -18446744073709551616.
  char* p = buffer + kfastint64tobufferoffset;
  *p-- = '\0';
  if (i >= 0) {
    do {
      *p-- = '0' + i % 10;
      i /= 10;
    } while (i > 0);
    return p + 1;
  } else {
    // on different platforms, % and / have different behaviors for
    // negative numbers, so we need to jump through hoops to make sure
    // we don't divide negative numbers.
    if (i > -10) {
      i = -i;
      *p-- = '0' + i;
      *p = '-';
      return p;
    } else {
      // make sure we aren't at min_int, in which case we can't say i = -i
      i = i + 10;
      i = -i;
      *p-- = '0' + i % 10;
      // undo what we did a moment ago
      i = i / 10 + 1;
      do {
        *p-- = '0' + i % 10;
        i /= 10;
      } while (i > 0);
      *p = '-';
      return p;
    }
  }
}

// offset into buffer where fastint32tobuffer places the end of string
// null character.  also used by fastint32tobufferleft
static const int kfastint32tobufferoffset = 11;

// yes, this is a duplicate of fastint64tobuffer.  but, we need this for the
// compiler to generate 32 bit arithmetic instructions.  it's much faster, at
// least with 32 bit binaries.
char *fastint32tobuffer(int32 i, char* buffer) {
  // we could collapse the positive and negative sections, but that
  // would be slightly slower for positive numbers...
  // 12 bytes is enough to store -2**32, -4294967296.
  char* p = buffer + kfastint32tobufferoffset;
  *p-- = '\0';
  if (i >= 0) {
    do {
      *p-- = '0' + i % 10;
      i /= 10;
    } while (i > 0);
    return p + 1;
  } else {
    // on different platforms, % and / have different behaviors for
    // negative numbers, so we need to jump through hoops to make sure
    // we don't divide negative numbers.
    if (i > -10) {
      i = -i;
      *p-- = '0' + i;
      *p = '-';
      return p;
    } else {
      // make sure we aren't at min_int, in which case we can't say i = -i
      i = i + 10;
      i = -i;
      *p-- = '0' + i % 10;
      // undo what we did a moment ago
      i = i / 10 + 1;
      do {
        *p-- = '0' + i % 10;
        i /= 10;
      } while (i > 0);
      *p = '-';
      return p;
    }
  }
}

char *fasthextobuffer(int i, char* buffer) {
  google_check(i >= 0) << "fasthextobuffer() wants non-negative integers, not " << i;

  static const char *hexdigits = "0123456789abcdef";
  char *p = buffer + 21;
  *p-- = '\0';
  do {
    *p-- = hexdigits[i & 15];   // mod by 16
    i >>= 4;                    // divide by 16
  } while (i > 0);
  return p + 1;
}

char *internalfasthextobuffer(uint64 value, char* buffer, int num_byte) {
  static const char *hexdigits = "0123456789abcdef";
  buffer[num_byte] = '\0';
  for (int i = num_byte - 1; i >= 0; i--) {
#ifdef _m_x64
    // msvc x64 platform has a bug optimizing the uint32(value) in the #else
    // block. given that the uint32 cast was to improve performance on 32-bit
    // platforms, we use 64-bit '&' directly.
    buffer[i] = hexdigits[value & 0xf];
#else
    buffer[i] = hexdigits[uint32(value) & 0xf];
#endif
    value >>= 4;
  }
  return buffer;
}

char *fasthex64tobuffer(uint64 value, char* buffer) {
  return internalfasthextobuffer(value, buffer, 16);
}

char *fasthex32tobuffer(uint32 value, char* buffer) {
  return internalfasthextobuffer(value, buffer, 8);
}

static inline char* placenum(char* p, int num, char prev_sep) {
   *p-- = '0' + num % 10;
   *p-- = '0' + num / 10;
   *p-- = prev_sep;
   return p;
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

static const char two_ascii_digits[100][2] = {
  {'0','0'}, {'0','1'}, {'0','2'}, {'0','3'}, {'0','4'},
  {'0','5'}, {'0','6'}, {'0','7'}, {'0','8'}, {'0','9'},
  {'1','0'}, {'1','1'}, {'1','2'}, {'1','3'}, {'1','4'},
  {'1','5'}, {'1','6'}, {'1','7'}, {'1','8'}, {'1','9'},
  {'2','0'}, {'2','1'}, {'2','2'}, {'2','3'}, {'2','4'},
  {'2','5'}, {'2','6'}, {'2','7'}, {'2','8'}, {'2','9'},
  {'3','0'}, {'3','1'}, {'3','2'}, {'3','3'}, {'3','4'},
  {'3','5'}, {'3','6'}, {'3','7'}, {'3','8'}, {'3','9'},
  {'4','0'}, {'4','1'}, {'4','2'}, {'4','3'}, {'4','4'},
  {'4','5'}, {'4','6'}, {'4','7'}, {'4','8'}, {'4','9'},
  {'5','0'}, {'5','1'}, {'5','2'}, {'5','3'}, {'5','4'},
  {'5','5'}, {'5','6'}, {'5','7'}, {'5','8'}, {'5','9'},
  {'6','0'}, {'6','1'}, {'6','2'}, {'6','3'}, {'6','4'},
  {'6','5'}, {'6','6'}, {'6','7'}, {'6','8'}, {'6','9'},
  {'7','0'}, {'7','1'}, {'7','2'}, {'7','3'}, {'7','4'},
  {'7','5'}, {'7','6'}, {'7','7'}, {'7','8'}, {'7','9'},
  {'8','0'}, {'8','1'}, {'8','2'}, {'8','3'}, {'8','4'},
  {'8','5'}, {'8','6'}, {'8','7'}, {'8','8'}, {'8','9'},
  {'9','0'}, {'9','1'}, {'9','2'}, {'9','3'}, {'9','4'},
  {'9','5'}, {'9','6'}, {'9','7'}, {'9','8'}, {'9','9'}
};

char* fastuint32tobufferleft(uint32 u, char* buffer) {
  int digits;
  const char *ascii_digits = null;
  // the idea of this implementation is to trim the number of divides to as few
  // as possible by using multiplication and subtraction rather than mod (%),
  // and by outputting two digits at a time rather than one.
  // the huge-number case is first, in the hopes that the compiler will output
  // that case in one branch-free block of code, and only output conditional
  // branches into it from below.
  if (u >= 1000000000) {  // >= 1,000,000,000
    digits = u / 100000000;  // 100,000,000
    ascii_digits = two_ascii_digits[digits];
    buffer[0] = ascii_digits[0];
    buffer[1] = ascii_digits[1];
    buffer += 2;
sublt100_000_000:
    u -= digits * 100000000;  // 100,000,000
lt100_000_000:
    digits = u / 1000000;  // 1,000,000
    ascii_digits = two_ascii_digits[digits];
    buffer[0] = ascii_digits[0];
    buffer[1] = ascii_digits[1];
    buffer += 2;
sublt1_000_000:
    u -= digits * 1000000;  // 1,000,000
lt1_000_000:
    digits = u / 10000;  // 10,000
    ascii_digits = two_ascii_digits[digits];
    buffer[0] = ascii_digits[0];
    buffer[1] = ascii_digits[1];
    buffer += 2;
sublt10_000:
    u -= digits * 10000;  // 10,000
lt10_000:
    digits = u / 100;
    ascii_digits = two_ascii_digits[digits];
    buffer[0] = ascii_digits[0];
    buffer[1] = ascii_digits[1];
    buffer += 2;
sublt100:
    u -= digits * 100;
lt100:
    digits = u;
    ascii_digits = two_ascii_digits[digits];
    buffer[0] = ascii_digits[0];
    buffer[1] = ascii_digits[1];
    buffer += 2;
done:
    *buffer = 0;
    return buffer;
  }

  if (u < 100) {
    digits = u;
    if (u >= 10) goto lt100;
    *buffer++ = '0' + digits;
    goto done;
  }
  if (u  <  10000) {   // 10,000
    if (u >= 1000) goto lt10_000;
    digits = u / 100;
    *buffer++ = '0' + digits;
    goto sublt100;
  }
  if (u  <  1000000) {   // 1,000,000
    if (u >= 100000) goto lt1_000_000;
    digits = u / 10000;  //    10,000
    *buffer++ = '0' + digits;
    goto sublt10_000;
  }
  if (u  <  100000000) {   // 100,000,000
    if (u >= 10000000) goto lt100_000_000;
    digits = u / 1000000;  //   1,000,000
    *buffer++ = '0' + digits;
    goto sublt1_000_000;
  }
  // we already know that u < 1,000,000,000
  digits = u / 100000000;   // 100,000,000
  *buffer++ = '0' + digits;
  goto sublt100_000_000;
}

char* fastint32tobufferleft(int32 i, char* buffer) {
  uint32 u = i;
  if (i < 0) {
    *buffer++ = '-';
    u = -i;
  }
  return fastuint32tobufferleft(u, buffer);
}

char* fastuint64tobufferleft(uint64 u64, char* buffer) {
  int digits;
  const char *ascii_digits = null;

  uint32 u = static_cast<uint32>(u64);
  if (u == u64) return fastuint32tobufferleft(u, buffer);

  uint64 top_11_digits = u64 / 1000000000;
  buffer = fastuint64tobufferleft(top_11_digits, buffer);
  u = u64 - (top_11_digits * 1000000000);

  digits = u / 10000000;  // 10,000,000
  google_dcheck_lt(digits, 100);
  ascii_digits = two_ascii_digits[digits];
  buffer[0] = ascii_digits[0];
  buffer[1] = ascii_digits[1];
  buffer += 2;
  u -= digits * 10000000;  // 10,000,000
  digits = u / 100000;  // 100,000
  ascii_digits = two_ascii_digits[digits];
  buffer[0] = ascii_digits[0];
  buffer[1] = ascii_digits[1];
  buffer += 2;
  u -= digits * 100000;  // 100,000
  digits = u / 1000;  // 1,000
  ascii_digits = two_ascii_digits[digits];
  buffer[0] = ascii_digits[0];
  buffer[1] = ascii_digits[1];
  buffer += 2;
  u -= digits * 1000;  // 1,000
  digits = u / 10;
  ascii_digits = two_ascii_digits[digits];
  buffer[0] = ascii_digits[0];
  buffer[1] = ascii_digits[1];
  buffer += 2;
  u -= digits * 10;
  digits = u;
  *buffer++ = '0' + digits;
  *buffer = 0;
  return buffer;
}

char* fastint64tobufferleft(int64 i, char* buffer) {
  uint64 u = i;
  if (i < 0) {
    *buffer++ = '-';
    u = -i;
  }
  return fastuint64tobufferleft(u, buffer);
}

// ----------------------------------------------------------------------
// simpleitoa()
//    description: converts an integer to a string.
//
//    return value: string
// ----------------------------------------------------------------------

string simpleitoa(int i) {
  char buffer[kfasttobuffersize];
  return (sizeof(i) == 4) ?
    fastint32tobuffer(i, buffer) :
    fastint64tobuffer(i, buffer);
}

string simpleitoa(unsigned int i) {
  char buffer[kfasttobuffersize];
  return string(buffer, (sizeof(i) == 4) ?
    fastuint32tobufferleft(i, buffer) :
    fastuint64tobufferleft(i, buffer));
}

string simpleitoa(long i) {
  char buffer[kfasttobuffersize];
  return (sizeof(i) == 4) ?
    fastint32tobuffer(i, buffer) :
    fastint64tobuffer(i, buffer);
}

string simpleitoa(unsigned long i) {
  char buffer[kfasttobuffersize];
  return string(buffer, (sizeof(i) == 4) ?
    fastuint32tobufferleft(i, buffer) :
    fastuint64tobufferleft(i, buffer));
}

string simpleitoa(long long i) {
  char buffer[kfasttobuffersize];
  return (sizeof(i) == 4) ?
    fastint32tobuffer(i, buffer) :
    fastint64tobuffer(i, buffer);
}

string simpleitoa(unsigned long long i) {
  char buffer[kfasttobuffersize];
  return string(buffer, (sizeof(i) == 4) ?
    fastuint32tobufferleft(i, buffer) :
    fastuint64tobufferleft(i, buffer));
}

// ----------------------------------------------------------------------
// simpledtoa()
// simpleftoa()
// doubletobuffer()
// floattobuffer()
//    we want to print the value without losing precision, but we also do
//    not want to print more digits than necessary.  this turns out to be
//    trickier than it sounds.  numbers like 0.2 cannot be represented
//    exactly in binary.  if we print 0.2 with a very large precision,
//    e.g. "%.50g", we get "0.2000000000000000111022302462515654042363167".
//    on the other hand, if we set the precision too low, we lose
//    significant digits when printing numbers that actually need them.
//    it turns out there is no precision value that does the right thing
//    for all numbers.
//
//    our strategy is to first try printing with a precision that is never
//    over-precise, then parse the result with strtod() to see if it
//    matches.  if not, we print again with a precision that will always
//    give a precise result, but may use more digits than necessary.
//
//    an arguably better strategy would be to use the algorithm described
//    in "how to print floating-point numbers accurately" by steele &
//    white, e.g. as implemented by david m. gay's dtoa().  it turns out,
//    however, that the following implementation is about as fast as
//    dmg's code.  furthermore, dmg's code locks mutexes, which means it
//    will not scale well on multi-core machines.  dmg's code is slightly
//    more accurate (in that it will never use more digits than
//    necessary), but this is probably irrelevant for most users.
//
//    rob pike and ken thompson also have an implementation of dtoa() in
//    third_party/fmt/fltfmt.cc.  their implementation is similar to this
//    one in that it makes guesses and then uses strtod() to check them.
//    their implementation is faster because they use their own code to
//    generate the digits in the first place rather than use snprintf(),
//    thus avoiding format string parsing overhead.  however, this makes
//    it considerably more complicated than the following implementation,
//    and it is embedded in a larger library.  if speed turns out to be
//    an issue, we could re-implement this in terms of their
//    implementation.
// ----------------------------------------------------------------------

string simpledtoa(double value) {
  char buffer[kdoubletobuffersize];
  return doubletobuffer(value, buffer);
}

string simpleftoa(float value) {
  char buffer[kfloattobuffersize];
  return floattobuffer(value, buffer);
}

static inline bool isvalidfloatchar(char c) {
  return ('0' <= c && c <= '9') ||
         c == 'e' || c == 'e' ||
         c == '+' || c == '-';
}

void delocalizeradix(char* buffer) {
  // fast check:  if the buffer has a normal decimal point, assume no
  // translation is needed.
  if (strchr(buffer, '.') != null) return;

  // find the first unknown character.
  while (isvalidfloatchar(*buffer)) ++buffer;

  if (*buffer == '\0') {
    // no radix character found.
    return;
  }

  // we are now pointing at the locale-specific radix character.  replace it
  // with '.'.
  *buffer = '.';
  ++buffer;

  if (!isvalidfloatchar(*buffer) && *buffer != '\0') {
    // it appears the radix was a multi-byte character.  we need to remove the
    // extra bytes.
    char* target = buffer;
    do { ++buffer; } while (!isvalidfloatchar(*buffer) && *buffer != '\0');
    memmove(target, buffer, strlen(buffer) + 1);
  }
}

char* doubletobuffer(double value, char* buffer) {
  // dbl_dig is 15 for ieee-754 doubles, which are used on almost all
  // platforms these days.  just in case some system exists where dbl_dig
  // is significantly larger -- and risks overflowing our buffer -- we have
  // this assert.
  google_compile_assert(dbl_dig < 20, dbl_dig_is_too_big);

  if (value == numeric_limits<double>::infinity()) {
    strcpy(buffer, "inf");
    return buffer;
  } else if (value == -numeric_limits<double>::infinity()) {
    strcpy(buffer, "-inf");
    return buffer;
  } else if (isnan(value)) {
    strcpy(buffer, "nan");
    return buffer;
  }

  int snprintf_result =
    snprintf(buffer, kdoubletobuffersize, "%.*g", dbl_dig, value);

  // the snprintf should never overflow because the buffer is significantly
  // larger than the precision we asked for.
  google_dcheck(snprintf_result > 0 && snprintf_result < kdoubletobuffersize);

  // we need to make parsed_value volatile in order to force the compiler to
  // write it out to the stack.  otherwise, it may keep the value in a
  // register, and if it does that, it may keep it as a long double instead
  // of a double.  this long double may have extra bits that make it compare
  // unequal to "value" even though it would be exactly equal if it were
  // truncated to a double.
  volatile double parsed_value = strtod(buffer, null);
  if (parsed_value != value) {
    int snprintf_result =
      snprintf(buffer, kdoubletobuffersize, "%.*g", dbl_dig+2, value);

    // should never overflow; see above.
    google_dcheck(snprintf_result > 0 && snprintf_result < kdoubletobuffersize);
  }

  delocalizeradix(buffer);
  return buffer;
}

bool safe_strtof(const char* str, float* value) {
  char* endptr;
  errno = 0;  // errno only gets set on errors
#if defined(_win32) || defined (__hpux)  // has no strtof()
  *value = strtod(str, &endptr);
#else
  *value = strtof(str, &endptr);
#endif
  return *str != 0 && *endptr == 0 && errno == 0;
}

char* floattobuffer(float value, char* buffer) {
  // flt_dig is 6 for ieee-754 floats, which are used on almost all
  // platforms these days.  just in case some system exists where flt_dig
  // is significantly larger -- and risks overflowing our buffer -- we have
  // this assert.
  google_compile_assert(flt_dig < 10, flt_dig_is_too_big);

  if (value == numeric_limits<double>::infinity()) {
    strcpy(buffer, "inf");
    return buffer;
  } else if (value == -numeric_limits<double>::infinity()) {
    strcpy(buffer, "-inf");
    return buffer;
  } else if (isnan(value)) {
    strcpy(buffer, "nan");
    return buffer;
  }

  int snprintf_result =
    snprintf(buffer, kfloattobuffersize, "%.*g", flt_dig, value);

  // the snprintf should never overflow because the buffer is significantly
  // larger than the precision we asked for.
  google_dcheck(snprintf_result > 0 && snprintf_result < kfloattobuffersize);

  float parsed_value;
  if (!safe_strtof(buffer, &parsed_value) || parsed_value != value) {
    int snprintf_result =
      snprintf(buffer, kfloattobuffersize, "%.*g", flt_dig+2, value);

    // should never overflow; see above.
    google_dcheck(snprintf_result > 0 && snprintf_result < kfloattobuffersize);
  }

  delocalizeradix(buffer);
  return buffer;
}

// ----------------------------------------------------------------------
// nolocalestrtod()
//   this code will make you cry.
// ----------------------------------------------------------------------

// returns a string identical to *input except that the character pointed to
// by radix_pos (which should be '.') is replaced with the locale-specific
// radix character.
string localizeradix(const char* input, const char* radix_pos) {
  // determine the locale-specific radix character by calling sprintf() to
  // print the number 1.5, then stripping off the digits.  as far as i can
  // tell, this is the only portable, thread-safe way to get the c library
  // to divuldge the locale's radix character.  no, localeconv() is not
  // thread-safe.
  char temp[16];
  int size = sprintf(temp, "%.1f", 1.5);
  google_check_eq(temp[0], '1');
  google_check_eq(temp[size-1], '5');
  google_check_le(size, 6);

  // now replace the '.' in the input with it.
  string result;
  result.reserve(strlen(input) + size - 3);
  result.append(input, radix_pos);
  result.append(temp + 1, size - 2);
  result.append(radix_pos + 1);
  return result;
}

double nolocalestrtod(const char* text, char** original_endptr) {
  // we cannot simply set the locale to "c" temporarily with setlocale()
  // as this is not thread-safe.  instead, we try to parse in the current
  // locale first.  if parsing stops at a '.' character, then this is a
  // pretty good hint that we're actually in some other locale in which
  // '.' is not the radix character.

  char* temp_endptr;
  double result = strtod(text, &temp_endptr);
  if (original_endptr != null) *original_endptr = temp_endptr;
  if (*temp_endptr != '.') return result;

  // parsing halted on a '.'.  perhaps we're in a different locale?  let's
  // try to replace the '.' with a locale-specific radix character and
  // try again.
  string localized = localizeradix(text, temp_endptr);
  const char* localized_cstr = localized.c_str();
  char* localized_endptr;
  result = strtod(localized_cstr, &localized_endptr);
  if ((localized_endptr - localized_cstr) >
      (temp_endptr - text)) {
    // this attempt got further, so replacing the decimal must have helped.
    // update original_endptr to point at the right location.
    if (original_endptr != null) {
      // size_diff is non-zero if the localized radix has multiple bytes.
      int size_diff = localized.size() - strlen(text);
      // const_cast is necessary to match the strtod() interface.
      *original_endptr = const_cast<char*>(
        text + (localized_endptr - localized_cstr - size_diff));
    }
  }

  return result;
}

}  // namespace protobuf
}  // namespace google
