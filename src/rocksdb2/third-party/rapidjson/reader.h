#ifndef rapidjson_reader_h_
#define rapidjson_reader_h_

// copyright (c) 2011 milo yip (miloyip@gmail.com)
// version 0.1

#include "rapidjson.h"
#include "internal/pow10.h"
#include "internal/stack.h"
#include <csetjmp>

#ifdef rapidjson_sse42
#include <nmmintrin.h>
#elif defined(rapidjson_sse2)
#include <emmintrin.h>
#endif

#ifdef _msc_ver
#pragma warning(push)
#pragma warning(disable : 4127) // conditional expression is constant
#endif

#ifndef rapidjson_parse_error
#define rapidjson_parse_error(msg, offset) \
	rapidjson_multilinemacro_begin \
	parseerror_ = msg; \
	erroroffset_ = offset; \
	longjmp(jmpbuf_, 1); \
	rapidjson_multilinemacro_end
#endif

namespace rapidjson {

///////////////////////////////////////////////////////////////////////////////
// parseflag

//! combination of parseflags
enum parseflag {
	kparsedefaultflags = 0,			//!< default parse flags. non-destructive parsing. text strings are decoded into allocated buffer.
	kparseinsituflag = 1			//!< in-situ(destructive) parsing.
};

///////////////////////////////////////////////////////////////////////////////
// handler

/*!	\class rapidjson::handler
	\brief concept for receiving events from genericreader upon parsing.
\code
concept handler {
	typename ch;

	void null();
	void bool(bool b);
	void int(int i);
	void uint(unsigned i);
	void int64(int64_t i);
	void uint64(uint64_t i);
	void double(double d);
	void string(const ch* str, sizetype length, bool copy);
	void startobject();
	void endobject(sizetype membercount);
	void startarray();
	void endarray(sizetype elementcount);
};
\endcode
*/
///////////////////////////////////////////////////////////////////////////////
// basereaderhandler

//! default implementation of handler.
/*! this can be used as base class of any reader handler.
	\implements handler
*/
template<typename encoding = utf8<> >
struct basereaderhandler {
	typedef typename encoding::ch ch;

	void default() {}
	void null() { default(); }
	void bool(bool) { default(); }
	void int(int) { default(); }
	void uint(unsigned) { default(); }
	void int64(int64_t) { default(); }
	void uint64(uint64_t) { default(); }
	void double(double) { default(); }
	void string(const ch*, sizetype, bool) { default(); }
	void startobject() { default(); }
	void endobject(sizetype) { default(); }
	void startarray() { default(); }
	void endarray(sizetype) { default(); }
};

///////////////////////////////////////////////////////////////////////////////
// skipwhitespace

//! skip the json white spaces in a stream.
/*! \param stream a input stream for skipping white spaces.
	\note this function has sse2/sse4.2 specialization.
*/
template<typename stream>
void skipwhitespace(stream& stream) {
	stream s = stream;	// use a local copy for optimization
	while (s.peek() == ' ' || s.peek() == '\n' || s.peek() == '\r' || s.peek() == '\t')
		s.take();
	stream = s;
}

#ifdef rapidjson_sse42
//! skip whitespace with sse 4.2 pcmpistrm instruction, testing 16 8-byte characters at once.
inline const char *skipwhitespace_simd(const char* p) {
	static const char whitespace[16] = " \n\r\t";
	__m128i w = _mm_loadu_si128((const __m128i *)&whitespace[0]);

	for (;;) {
		__m128i s = _mm_loadu_si128((const __m128i *)p);
		unsigned r = _mm_cvtsi128_si32(_mm_cmpistrm(w, s, _sidd_ubyte_ops | _sidd_cmp_equal_any | _sidd_bit_mask | _sidd_negative_polarity));
		if (r == 0)	// all 16 characters are whitespace
			p += 16;
		else {		// some of characters may be non-whitespace
#ifdef _msc_ver		// find the index of first non-whitespace
			unsigned long offset;
			if (_bitscanforward(&offset, r))
				return p + offset;
#else
			if (r != 0)
				return p + __builtin_ffs(r) - 1;
#endif
		}
	}
}

#elif defined(rapidjson_sse2)

//! skip whitespace with sse2 instructions, testing 16 8-byte characters at once.
inline const char *skipwhitespace_simd(const char* p) {
	static const char whitespaces[4][17] = {
		"                ",
		"\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n",
		"\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r",
		"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"};

	__m128i w0 = _mm_loadu_si128((const __m128i *)&whitespaces[0][0]);
	__m128i w1 = _mm_loadu_si128((const __m128i *)&whitespaces[1][0]);
	__m128i w2 = _mm_loadu_si128((const __m128i *)&whitespaces[2][0]);
	__m128i w3 = _mm_loadu_si128((const __m128i *)&whitespaces[3][0]);

	for (;;) {
		__m128i s = _mm_loadu_si128((const __m128i *)p);
		__m128i x = _mm_cmpeq_epi8(s, w0);
		x = _mm_or_si128(x, _mm_cmpeq_epi8(s, w1));
		x = _mm_or_si128(x, _mm_cmpeq_epi8(s, w2));
		x = _mm_or_si128(x, _mm_cmpeq_epi8(s, w3));
		unsigned short r = ~_mm_movemask_epi8(x);
		if (r == 0)	// all 16 characters are whitespace
			p += 16;
		else {		// some of characters may be non-whitespace
#ifdef _msc_ver		// find the index of first non-whitespace
			unsigned long offset;
			if (_bitscanforward(&offset, r))
				return p + offset;
#else
			if (r != 0)
				return p + __builtin_ffs(r) - 1;
#endif
		}
	}
}

#endif // rapidjson_sse2

#ifdef rapidjson_simd
//! template function specialization for insitustringstream
template<> inline void skipwhitespace(insitustringstream& stream) { 
	stream.src_ = const_cast<char*>(skipwhitespace_simd(stream.src_));
}

//! template function specialization for stringstream
template<> inline void skipwhitespace(stringstream& stream) {
	stream.src_ = skipwhitespace_simd(stream.src_);
}
#endif // rapidjson_simd

///////////////////////////////////////////////////////////////////////////////
// genericreader

//! sax-style json parser. use reader for utf8 encoding and default allocator.
/*! genericreader parses json text from a stream, and send events synchronously to an 
    object implementing handler concept.

    it needs to allocate a stack for storing a single decoded string during 
    non-destructive parsing.

    for in-situ parsing, the decoded string is directly written to the source 
    text string, no temporary buffer is required.

    a genericreader object can be reused for parsing multiple json text.
    
    \tparam encoding encoding of both the stream and the parse output.
    \tparam allocator allocator type for stack.
*/
template <typename encoding, typename allocator = memorypoolallocator<> >
class genericreader {
public:
	typedef typename encoding::ch ch;

	//! constructor.
	/*! \param allocator optional allocator for allocating stack memory. (only use for non-destructive parsing)
		\param stackcapacity stack capacity in bytes for storing a single decoded string.  (only use for non-destructive parsing)
	*/
	genericreader(allocator* allocator = 0, size_t stackcapacity = kdefaultstackcapacity) : stack_(allocator, stackcapacity), parseerror_(0), erroroffset_(0) {}

	//! parse json text.
	/*! \tparam parseflags combination of parseflag. 
		 \tparam stream type of input stream.
		 \tparam handler type of handler which must implement handler concept.
		 \param stream input stream to be parsed.
		 \param handler the handler to receive events.
		 \return whether the parsing is successful.
	*/
	template <unsigned parseflags, typename stream, typename handler>
	bool parse(stream& stream, handler& handler) {
		parseerror_ = 0;
		erroroffset_ = 0;

#ifdef _msc_ver
#pragma warning(push)
#pragma warning(disable : 4611) // interaction between '_setjmp' and c++ object destruction is non-portable
#endif
		if (setjmp(jmpbuf_)) {
#ifdef _msc_ver
#pragma warning(pop)
#endif
			stack_.clear();
			return false;
		}

		skipwhitespace(stream);

		if (stream.peek() == '\0')
			rapidjson_parse_error("text only contains white space(s)", stream.tell());
		else {
			switch (stream.peek()) {
				case '{': parseobject<parseflags>(stream, handler); break;
				case '[': parsearray<parseflags>(stream, handler); break;
				default: rapidjson_parse_error("expect either an object or array at root", stream.tell());
			}
			skipwhitespace(stream);

			if (stream.peek() != '\0')
				rapidjson_parse_error("nothing should follow the root object or array.", stream.tell());
		}

		return true;
	}

	bool hasparseerror() const { return parseerror_ != 0; }
	const char* getparseerror() const { return parseerror_; }
	size_t geterroroffset() const { return erroroffset_; }

private:
	// parse object: { string : value, ... }
	template<unsigned parseflags, typename stream, typename handler>
	void parseobject(stream& stream, handler& handler) {
		rapidjson_assert(stream.peek() == '{');
		stream.take();	// skip '{'
		handler.startobject();
		skipwhitespace(stream);

		if (stream.peek() == '}') {
			stream.take();
			handler.endobject(0);	// empty object
			return;
		}

		for (sizetype membercount = 0;;) {
			if (stream.peek() != '"') {
				rapidjson_parse_error("name of an object member must be a string", stream.tell());
				break;
			}

			parsestring<parseflags>(stream, handler);
			skipwhitespace(stream);

			if (stream.take() != ':') {
				rapidjson_parse_error("there must be a colon after the name of object member", stream.tell());
				break;
			}
			skipwhitespace(stream);

			parsevalue<parseflags>(stream, handler);
			skipwhitespace(stream);

			++membercount;

			switch(stream.take()) {
				case ',': skipwhitespace(stream); break;
				case '}': handler.endobject(membercount); return;
				default:  rapidjson_parse_error("must be a comma or '}' after an object member", stream.tell());
			}
		}
	}

	// parse array: [ value, ... ]
	template<unsigned parseflags, typename stream, typename handler>
	void parsearray(stream& stream, handler& handler) {
		rapidjson_assert(stream.peek() == '[');
		stream.take();	// skip '['
		handler.startarray();
		skipwhitespace(stream);

		if (stream.peek() == ']') {
			stream.take();
			handler.endarray(0); // empty array
			return;
		}

		for (sizetype elementcount = 0;;) {
			parsevalue<parseflags>(stream, handler);
			++elementcount;
			skipwhitespace(stream);

			switch (stream.take()) {
				case ',': skipwhitespace(stream); break;
				case ']': handler.endarray(elementcount); return;
				default:  rapidjson_parse_error("must be a comma or ']' after an array element.", stream.tell());
			}
		}
	}

	template<unsigned parseflags, typename stream, typename handler>
	void parsenull(stream& stream, handler& handler) {
		rapidjson_assert(stream.peek() == 'n');
		stream.take();

		if (stream.take() == 'u' && stream.take() == 'l' && stream.take() == 'l')
			handler.null();
		else
			rapidjson_parse_error("invalid value", stream.tell() - 1);
	}

	template<unsigned parseflags, typename stream, typename handler>
	void parsetrue(stream& stream, handler& handler) {
		rapidjson_assert(stream.peek() == 't');
		stream.take();

		if (stream.take() == 'r' && stream.take() == 'u' && stream.take() == 'e')
			handler.bool(true);
		else
			rapidjson_parse_error("invalid value", stream.tell());
	}

	template<unsigned parseflags, typename stream, typename handler>
	void parsefalse(stream& stream, handler& handler) {
		rapidjson_assert(stream.peek() == 'f');
		stream.take();

		if (stream.take() == 'a' && stream.take() == 'l' && stream.take() == 's' && stream.take() == 'e')
			handler.bool(false);
		else
			rapidjson_parse_error("invalid value", stream.tell() - 1);
	}

	// helper function to parse four hexidecimal digits in \uxxxx in parsestring().
	template<typename stream>
	unsigned parsehex4(stream& stream) {
		stream s = stream;	// use a local copy for optimization
		unsigned codepoint = 0;
		for (int i = 0; i < 4; i++) {
			ch c = s.take();
			codepoint <<= 4;
			codepoint += c;
			if (c >= '0' && c <= '9')
				codepoint -= '0';
			else if (c >= 'a' && c <= 'f')
				codepoint -= 'a' - 10;
			else if (c >= 'a' && c <= 'f')
				codepoint -= 'a' - 10;
			else 
				rapidjson_parse_error("incorrect hex digit after \\u escape", s.tell() - 1);
		}
		stream = s; // restore stream
		return codepoint;
	}

	// parse string, handling the prefix and suffix double quotes and escaping.
	template<unsigned parseflags, typename stream, typename handler>
	void parsestring(stream& stream, handler& handler) {
#define z16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
		static const ch escape[256] = {
			z16, z16, 0, 0,'\"', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,'/', 
			z16, z16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,'\\', 0, 0, 0, 
			0, 0,'\b', 0, 0, 0,'\f', 0, 0, 0, 0, 0, 0, 0,'\n', 0, 
			0, 0,'\r', 0,'\t', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
			z16, z16, z16, z16, z16, z16, z16, z16
		};
#undef z16

		stream s = stream;	// use a local copy for optimization
		rapidjson_assert(s.peek() == '\"');
		s.take();	// skip '\"'
		ch *head;
		sizetype len;
		if (parseflags & kparseinsituflag)
			head = s.putbegin();
		else
			len = 0;

#define rapidjson_put(x) \
	do { \
		if (parseflags & kparseinsituflag) \
			s.put(x); \
		else { \
			*stack_.template push<ch>() = x; \
			++len; \
		} \
	} while(false)

		for (;;) {
			ch c = s.take();
			if (c == '\\') {	// escape
				ch e = s.take();
				if ((sizeof(ch) == 1 || (int)e < 256) && escape[(unsigned char)e])
					rapidjson_put(escape[(unsigned char)e]);
				else if (e == 'u') {	// unicode
					unsigned codepoint = parsehex4(s);
					if (codepoint >= 0xd800 && codepoint <= 0xdbff) { // handle utf-16 surrogate pair
						if (s.take() != '\\' || s.take() != 'u') {
							rapidjson_parse_error("missing the second \\u in surrogate pair", s.tell() - 2);
							return;
						}
						unsigned codepoint2 = parsehex4(s);
						if (codepoint2 < 0xdc00 || codepoint2 > 0xdfff) {
							rapidjson_parse_error("the second \\u in surrogate pair is invalid", s.tell() - 2);
							return;
						}
						codepoint = (((codepoint - 0xd800) << 10) | (codepoint2 - 0xdc00)) + 0x10000;
					}

					ch buffer[4];
					sizetype count = sizetype(encoding::encode(buffer, codepoint) - &buffer[0]);

					if (parseflags & kparseinsituflag) 
						for (sizetype i = 0; i < count; i++)
							s.put(buffer[i]);
					else {
						memcpy(stack_.template push<ch>(count), buffer, count * sizeof(ch));
						len += count;
					}
				}
				else {
					rapidjson_parse_error("unknown escape character", stream.tell() - 1);
					return;
				}
			}
			else if (c == '"') {	// closing double quote
				if (parseflags & kparseinsituflag) {
					size_t length = s.putend(head);
					rapidjson_assert(length <= 0xffffffff);
					rapidjson_put('\0');	// null-terminate the string
					handler.string(head, sizetype(length), false);
				}
				else {
					rapidjson_put('\0');
					handler.string(stack_.template pop<ch>(len), len - 1, true);
				}
				stream = s;	// restore stream
				return;
			}
			else if (c == '\0') {
				rapidjson_parse_error("lacks ending quotation before the end of string", stream.tell() - 1);
				return;
			}
			else if ((unsigned)c < 0x20) {	// rfc 4627: unescaped = %x20-21 / %x23-5b / %x5d-10ffff
				rapidjson_parse_error("incorrect unescaped character in string", stream.tell() - 1);
				return;
			}
			else
				rapidjson_put(c);	// normal character, just copy
		}
#undef rapidjson_put
	}

	template<unsigned parseflags, typename stream, typename handler>
	void parsenumber(stream& stream, handler& handler) {
		stream s = stream; // local copy for optimization
		// parse minus
		bool minus = false;
		if (s.peek() == '-') {
			minus = true;
			s.take();
		}

		// parse int: zero / ( digit1-9 *digit )
		unsigned i;
		bool try64bit = false;
		if (s.peek() == '0') {
			i = 0;
			s.take();
		}
		else if (s.peek() >= '1' && s.peek() <= '9') {
			i = s.take() - '0';

			if (minus)
				while (s.peek() >= '0' && s.peek() <= '9') {
					if (i >= 214748364) { // 2^31 = 2147483648
						if (i != 214748364 || s.peek() > '8') {
							try64bit = true;
							break;
						}
					}
					i = i * 10 + (s.take() - '0');
				}
			else
				while (s.peek() >= '0' && s.peek() <= '9') {
					if (i >= 429496729) { // 2^32 - 1 = 4294967295
						if (i != 429496729 || s.peek() > '5') {
							try64bit = true;
							break;
						}
					}
					i = i * 10 + (s.take() - '0');
				}
		}
		else {
			rapidjson_parse_error("expect a value here.", stream.tell());
			return;
		}

		// parse 64bit int
		uint64_t i64 = 0;
		bool usedouble = false;
		if (try64bit) {
			i64 = i;
			if (minus) 
				while (s.peek() >= '0' && s.peek() <= '9') {					
					if (i64 >= 922337203685477580ull) // 2^63 = 9223372036854775808
						if (i64 != 922337203685477580ull || s.peek() > '8') {
							usedouble = true;
							break;
						}
					i64 = i64 * 10 + (s.take() - '0');
				}
			else
				while (s.peek() >= '0' && s.peek() <= '9') {					
					if (i64 >= 1844674407370955161ull) // 2^64 - 1 = 18446744073709551615
						if (i64 != 1844674407370955161ull || s.peek() > '5') {
							usedouble = true;
							break;
						}
					i64 = i64 * 10 + (s.take() - '0');
				}
		}

		// force double for big integer
		double d = 0.0;
		if (usedouble) {
			d = (double)i64;
			while (s.peek() >= '0' && s.peek() <= '9') {
				if (d >= 1e307) {
					rapidjson_parse_error("number too big to store in double", stream.tell());
					return;
				}
				d = d * 10 + (s.take() - '0');
			}
		}

		// parse frac = decimal-point 1*digit
		int expfrac = 0;
		if (s.peek() == '.') {
			if (!usedouble) {
				d = try64bit ? (double)i64 : (double)i;
				usedouble = true;
			}
			s.take();

			if (s.peek() >= '0' && s.peek() <= '9') {
				d = d * 10 + (s.take() - '0');
				--expfrac;
			}
			else {
				rapidjson_parse_error("at least one digit in fraction part", stream.tell());
				return;
			}

			while (s.peek() >= '0' && s.peek() <= '9') {
				if (expfrac > -16) {
					d = d * 10 + (s.peek() - '0');
					--expfrac;
				}
				s.take();
			}
		}

		// parse exp = e [ minus / plus ] 1*digit
		int exp = 0;
		if (s.peek() == 'e' || s.peek() == 'e') {
			if (!usedouble) {
				d = try64bit ? (double)i64 : (double)i;
				usedouble = true;
			}
			s.take();

			bool expminus = false;
			if (s.peek() == '+')
				s.take();
			else if (s.peek() == '-') {
				s.take();
				expminus = true;
			}

			if (s.peek() >= '0' && s.peek() <= '9') {
				exp = s.take() - '0';
				while (s.peek() >= '0' && s.peek() <= '9') {
					exp = exp * 10 + (s.take() - '0');
					if (exp > 308) {
						rapidjson_parse_error("number too big to store in double", stream.tell());
						return;
					}
				}
			}
			else {
				rapidjson_parse_error("at least one digit in exponent", s.tell());
				return;
			}

			if (expminus)
				exp = -exp;
		}

		// finish parsing, call event according to the type of number.
		if (usedouble) {
			d *= internal::pow10(exp + expfrac);
			handler.double(minus ? -d : d);
		}
		else {
			if (try64bit) {
				if (minus)
					handler.int64(-(int64_t)i64);
				else
					handler.uint64(i64);
			}
			else {
				if (minus)
					handler.int(-(int)i);
				else
					handler.uint(i);
			}
		}

		stream = s; // restore stream
	}

	// parse any json value
	template<unsigned parseflags, typename stream, typename handler>
	void parsevalue(stream& stream, handler& handler) {
		switch (stream.peek()) {
			case 'n': parsenull  <parseflags>(stream, handler); break;
			case 't': parsetrue  <parseflags>(stream, handler); break;
			case 'f': parsefalse <parseflags>(stream, handler); break;
			case '"': parsestring<parseflags>(stream, handler); break;
			case '{': parseobject<parseflags>(stream, handler); break;
			case '[': parsearray <parseflags>(stream, handler); break;
			default : parsenumber<parseflags>(stream, handler);
		}
	}

	static const size_t kdefaultstackcapacity = 256;	//!< default stack capacity in bytes for storing a single decoded string. 
	internal::stack<allocator> stack_;	//!< a stack for storing decoded string temporarily during non-destructive parsing.
	jmp_buf jmpbuf_;					//!< setjmp buffer for fast exit from nested parsing function calls.
	const char* parseerror_;
	size_t erroroffset_;
}; // class genericreader

//! reader with utf8 encoding and default allocator.
typedef genericreader<utf8<> > reader;

} // namespace rapidjson

#ifdef _msc_ver
#pragma warning(pop)
#endif

#endif // rapidjson_reader_h_
