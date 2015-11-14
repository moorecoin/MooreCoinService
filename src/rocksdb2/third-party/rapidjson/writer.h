#ifndef rapidjson_writer_h_
#define rapidjson_writer_h_

#include "rapidjson.h"
#include "internal/stack.h"
#include "internal/strfunc.h"
#include <cstdio>	// snprintf() or _sprintf_s()
#include <new>		// placement new

#ifdef _msc_ver
#pragma warning(push)
#pragma warning(disable : 4127) // conditional expression is constant
#endif

namespace rapidjson {

//! json writer
/*! writer implements the concept handler.
	it generates json text by events to an output stream.

	user may programmatically calls the functions of a writer to generate json text.

	on the other side, a writer can also be passed to objects that generates events, 

	for example reader::parse() and document::accept().

	\tparam stream type of ouptut stream.
	\tparam encoding encoding of both source strings and output.
	\implements handler
*/
template<typename stream, typename encoding = utf8<>, typename allocator = memorypoolallocator<> >
class writer {
public:
	typedef typename encoding::ch ch;

	writer(stream& stream, allocator* allocator = 0, size_t leveldepth = kdefaultleveldepth) : 
		stream_(stream), level_stack_(allocator, leveldepth * sizeof(level)) {}

	//@name implementation of handler
	//@{
	writer& null()					{ prefix(knulltype);   writenull();			return *this; }
	writer& bool(bool b)			{ prefix(b ? ktruetype : kfalsetype); writebool(b); return *this; }
	writer& int(int i)				{ prefix(knumbertype); writeint(i);			return *this; }
	writer& uint(unsigned u)		{ prefix(knumbertype); writeuint(u);		return *this; }
	writer& int64(int64_t i64)		{ prefix(knumbertype); writeint64(i64);		return *this; }
	writer& uint64(uint64_t u64)	{ prefix(knumbertype); writeuint64(u64);	return *this; }
	writer& double(double d)		{ prefix(knumbertype); writedouble(d);		return *this; }

	writer& string(const ch* str, sizetype length, bool copy = false) {
		(void)copy;
		prefix(kstringtype);
		writestring(str, length);
		return *this;
	}

	writer& startobject() {
		prefix(kobjecttype);
		new (level_stack_.template push<level>()) level(false);
		writestartobject();
		return *this;
	}

	writer& endobject(sizetype membercount = 0) {
		(void)membercount;
		rapidjson_assert(level_stack_.getsize() >= sizeof(level));
		rapidjson_assert(!level_stack_.template top<level>()->inarray);
		level_stack_.template pop<level>(1);
		writeendobject();
		return *this;
	}

	writer& startarray() {
		prefix(karraytype);
		new (level_stack_.template push<level>()) level(true);
		writestartarray();
		return *this;
	}

	writer& endarray(sizetype elementcount = 0) {
		(void)elementcount;
		rapidjson_assert(level_stack_.getsize() >= sizeof(level));
		rapidjson_assert(level_stack_.template top<level>()->inarray);
		level_stack_.template pop<level>(1);
		writeendarray();
		return *this;
	}
	//@}

	//! simpler but slower overload.
	writer& string(const ch* str) { return string(str, internal::strlen(str)); }

protected:
	//! information for each nested level
	struct level {
		level(bool inarray_) : inarray(inarray_), valuecount(0) {}
		bool inarray;		//!< true if in array, otherwise in object
		size_t valuecount;	//!< number of values in this level
	};

	static const size_t kdefaultleveldepth = 32;

	void writenull()  {
		stream_.put('n'); stream_.put('u'); stream_.put('l'); stream_.put('l');
	}

	void writebool(bool b)  {
		if (b) {
			stream_.put('t'); stream_.put('r'); stream_.put('u'); stream_.put('e');
		}
		else {
			stream_.put('f'); stream_.put('a'); stream_.put('l'); stream_.put('s'); stream_.put('e');
		}
	}

	void writeint(int i) {
		if (i < 0) {
			stream_.put('-');
			i = -i;
		}
		writeuint((unsigned)i);
	}

	void writeuint(unsigned u) {
		char buffer[10];
		char *p = buffer;
		do {
			*p++ = (u % 10) + '0';
			u /= 10;
		} while (u > 0);

		do {
			--p;
			stream_.put(*p);
		} while (p != buffer);
	}

	void writeint64(int64_t i64) {
		if (i64 < 0) {
			stream_.put('-');
			i64 = -i64;
		}
		writeuint64((uint64_t)i64);
	}

	void writeuint64(uint64_t u64) {
		char buffer[20];
		char *p = buffer;
		do {
			*p++ = char(u64 % 10) + '0';
			u64 /= 10;
		} while (u64 > 0);

		do {
			--p;
			stream_.put(*p);
		} while (p != buffer);
	}

	//! \todo optimization with custom double-to-string converter.
	void writedouble(double d) {
		char buffer[100];
#if _msc_ver
		int ret = sprintf_s(buffer, sizeof(buffer), "%g", d);
#else
		int ret = snprintf(buffer, sizeof(buffer), "%g", d);
#endif
		rapidjson_assert(ret >= 1);
		for (int i = 0; i < ret; i++)
			stream_.put(buffer[i]);
	}

	void writestring(const ch* str, sizetype length)  {
		static const char hexdigits[] = "0123456789abcdef";
		static const char escape[256] = {
#define z16 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
			//0    1    2    3    4    5    6    7    8    9    a    b    c    d    e    f
			'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'b', 't', 'n', 'u', 'f', 'r', 'u', 'u', // 00
			'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u', // 10
			  0,   0, '"',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, // 20
			z16, z16,																		// 30~4f
			  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\\',   0,   0,   0, // 50
			z16, z16, z16, z16, z16, z16, z16, z16, z16, z16								// 60~ff
#undef z16
		};

		stream_.put('\"');
		for (const ch* p = str; p != str + length; ++p) {
			if ((sizeof(ch) == 1 || *p < 256) && escape[(unsigned char)*p])  {
				stream_.put('\\');
				stream_.put(escape[(unsigned char)*p]);
				if (escape[(unsigned char)*p] == 'u') {
					stream_.put('0');
					stream_.put('0');
					stream_.put(hexdigits[(*p) >> 4]);
					stream_.put(hexdigits[(*p) & 0xf]);
				}
			}
			else
				stream_.put(*p);
		}
		stream_.put('\"');
	}

	void writestartobject()	{ stream_.put('{'); }
	void writeendobject()	{ stream_.put('}'); }
	void writestartarray()	{ stream_.put('['); }
	void writeendarray()	{ stream_.put(']'); }

	void prefix(type type) {
		(void)type;
		if (level_stack_.getsize() != 0) { // this value is not at root
			level* level = level_stack_.template top<level>();
			if (level->valuecount > 0) {
				if (level->inarray) 
					stream_.put(','); // add comma if it is not the first element in array
				else  // in object
					stream_.put((level->valuecount % 2 == 0) ? ',' : ':');
			}
			if (!level->inarray && level->valuecount % 2 == 0)
				rapidjson_assert(type == kstringtype);  // if it's in object, then even number should be a name
			level->valuecount++;
		}
		else
			rapidjson_assert(type == kobjecttype || type == karraytype);
	}

	stream& stream_;
	internal::stack<allocator> level_stack_;

private:
	// prohibit assignment for vc c4512 warning
	writer& operator=(const writer& w);
};

} // namespace rapidjson

#ifdef _msc_ver
#pragma warning(pop)
#endif

#endif // rapidjson_rapidjson_h_
