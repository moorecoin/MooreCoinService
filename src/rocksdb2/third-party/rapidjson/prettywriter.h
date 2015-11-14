#ifndef rapidjson_prettywriter_h_
#define rapidjson_prettywriter_h_

#include "writer.h"

namespace rapidjson {

//! writer with indentation and spacing.
/*!
	\tparam stream type of ouptut stream.
	\tparam encoding encoding of both source strings and output.
	\tparam allocator type of allocator for allocating memory of stack.
*/
template<typename stream, typename encoding = utf8<>, typename allocator = memorypoolallocator<> >
class prettywriter : public writer<stream, encoding, allocator> {
public:
	typedef writer<stream, encoding, allocator> base;
	typedef typename base::ch ch;

	//! constructor
	/*! \param stream output stream.
		\param allocator user supplied allocator. if it is null, it will create a private one.
		\param leveldepth initial capacity of 
	*/
	prettywriter(stream& stream, allocator* allocator = 0, size_t leveldepth = base::kdefaultleveldepth) : 
		base(stream, allocator, leveldepth), indentchar_(' '), indentcharcount_(4) {}

	//! set custom indentation.
	/*! \param indentchar		character for indentation. must be whitespace character (' ', '\t', '\n', '\r').
		\param indentcharcount	number of indent characters for each indentation level.
		\note the default indentation is 4 spaces.
	*/
	prettywriter& setindent(ch indentchar, unsigned indentcharcount) {
		rapidjson_assert(indentchar == ' ' || indentchar == '\t' || indentchar == '\n' || indentchar == '\r');
		indentchar_ = indentchar;
		indentcharcount_ = indentcharcount;
		return *this;
	}

	//@name implementation of handler.
	//@{

	prettywriter& null()				{ prettyprefix(knulltype);   base::writenull();			return *this; }
	prettywriter& bool(bool b)			{ prettyprefix(b ? ktruetype : kfalsetype); base::writebool(b); return *this; }
	prettywriter& int(int i)			{ prettyprefix(knumbertype); base::writeint(i);			return *this; }
	prettywriter& uint(unsigned u)		{ prettyprefix(knumbertype); base::writeuint(u);		return *this; }
	prettywriter& int64(int64_t i64)	{ prettyprefix(knumbertype); base::writeint64(i64);		return *this; }
	prettywriter& uint64(uint64_t u64)	{ prettyprefix(knumbertype); base::writeuint64(u64);	return *this; }
	prettywriter& double(double d)		{ prettyprefix(knumbertype); base::writedouble(d);		return *this; }

	prettywriter& string(const ch* str, sizetype length, bool copy = false) {
		(void)copy;
		prettyprefix(kstringtype);
		base::writestring(str, length);
		return *this;
	}

	prettywriter& startobject() {
		prettyprefix(kobjecttype);
		new (base::level_stack_.template push<typename base::level>()) typename base::level(false);
		base::writestartobject();
		return *this;
	}

	prettywriter& endobject(sizetype membercount = 0) {
		(void)membercount;
		rapidjson_assert(base::level_stack_.getsize() >= sizeof(typename base::level));
		rapidjson_assert(!base::level_stack_.template top<typename base::level>()->inarray);
		bool empty = base::level_stack_.template pop<typename base::level>(1)->valuecount == 0;

		if (!empty) {
			base::stream_.put('\n');
			writeindent();
		}
		base::writeendobject();
		return *this;
	}

	prettywriter& startarray() {
		prettyprefix(karraytype);
		new (base::level_stack_.template push<typename base::level>()) typename base::level(true);
		base::writestartarray();
		return *this;
	}

	prettywriter& endarray(sizetype membercount = 0) {
		(void)membercount;
		rapidjson_assert(base::level_stack_.getsize() >= sizeof(typename base::level));
		rapidjson_assert(base::level_stack_.template top<typename base::level>()->inarray);
		bool empty = base::level_stack_.template pop<typename base::level>(1)->valuecount == 0;

		if (!empty) {
			base::stream_.put('\n');
			writeindent();
		}
		base::writeendarray();
		return *this;
	}

	//@}

	//! simpler but slower overload.
	prettywriter& string(const ch* str) { return string(str, internal::strlen(str)); }

protected:
	void prettyprefix(type type) {
		(void)type;
		if (base::level_stack_.getsize() != 0) { // this value is not at root
			typename base::level* level = base::level_stack_.template top<typename base::level>();

			if (level->inarray) {
				if (level->valuecount > 0) {
					base::stream_.put(','); // add comma if it is not the first element in array
					base::stream_.put('\n');
				}
				else
					base::stream_.put('\n');
				writeindent();
			}
			else {	// in object
				if (level->valuecount > 0) {
					if (level->valuecount % 2 == 0) {
						base::stream_.put(',');
						base::stream_.put('\n');
					}
					else {
						base::stream_.put(':');
						base::stream_.put(' ');
					}
				}
				else
					base::stream_.put('\n');

				if (level->valuecount % 2 == 0)
					writeindent();
			}
			if (!level->inarray && level->valuecount % 2 == 0)
				rapidjson_assert(type == kstringtype);  // if it's in object, then even number should be a name
			level->valuecount++;
		}
		else
			rapidjson_assert(type == kobjecttype || type == karraytype);
	}

	void writeindent()  {
		size_t count = (base::level_stack_.getsize() / sizeof(typename base::level)) * indentcharcount_;
		putn(base::stream_, indentchar_, count);
	}

	ch indentchar_;
	unsigned indentcharcount_;
};

} // namespace rapidjson

#endif // rapidjson_rapidjson_h_
