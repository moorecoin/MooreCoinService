#ifndef rapidjson_document_h_
#define rapidjson_document_h_

#include "reader.h"
#include "internal/strfunc.h"
#include <new>		// placement new

#ifdef _msc_ver
#pragma warning(push)
#pragma warning(disable : 4127) // conditional expression is constant
#endif

namespace rapidjson {

///////////////////////////////////////////////////////////////////////////////
// genericvalue

//! represents a json value. use value for utf8 encoding and default allocator.
/*!
	a json value can be one of 7 types. this class is a variant type supporting
	these types.

	use the value if utf8 and default allocator

	\tparam encoding	encoding of the value. (even non-string values need to have the same encoding in a document)
	\tparam allocator	allocator type for allocating memory of object, array and string.
*/
#pragma pack (push, 4)
template <typename encoding, typename allocator = memorypoolallocator<> > 
class genericvalue {
public:
	//! name-value pair in an object.
	struct member { 
		genericvalue<encoding, allocator> name;		//!< name of member (must be a string)
		genericvalue<encoding, allocator> value;	//!< value of member.
	};

	typedef encoding encodingtype;					//!< encoding type from template parameter.
	typedef allocator allocatortype;				//!< allocator type from template parameter.
	typedef typename encoding::ch ch;				//!< character type derived from encoding.
	typedef member* memberiterator;					//!< member iterator for iterating in object.
	typedef const member* constmemberiterator;		//!< constant member iterator for iterating in object.
	typedef genericvalue* valueiterator;			//!< value iterator for iterating in array.
	typedef const genericvalue* constvalueiterator;	//!< constant value iterator for iterating in array.

	//!@name constructors and destructor.
	//@{

	//! default constructor creates a null value.
	genericvalue() : flags_(knullflag) {}

	//! copy constructor is not permitted.
private:
	genericvalue(const genericvalue& rhs);

public:

	//! constructor with json value type.
	/*! this creates a value of specified type with default content.
		\param type	type of the value.
		\note default content for number is zero.
	*/
	genericvalue(type type) {
		static const unsigned defaultflags[7] = {
			knullflag, kfalseflag, ktrueflag, kobjectflag, karrayflag, kconststringflag,
			knumberflag | kintflag | kuintflag | kint64flag | kuint64flag | kdoubleflag
		};
		rapidjson_assert(type <= knumbertype);
		flags_ = defaultflags[type];
		memset(&data_, 0, sizeof(data_));
	}

	//! constructor for boolean value.
	genericvalue(bool b) : flags_(b ? ktrueflag : kfalseflag) {}

	//! constructor for int value.
	genericvalue(int i) : flags_(knumberintflag) { 
		data_.n.i64 = i;
		if (i >= 0)
			flags_ |= kuintflag | kuint64flag;
	}

	//! constructor for unsigned value.
	genericvalue(unsigned u) : flags_(knumberuintflag) {
		data_.n.u64 = u; 
		if (!(u & 0x80000000))
			flags_ |= kintflag | kint64flag;
	}

	//! constructor for int64_t value.
	genericvalue(int64_t i64) : flags_(knumberint64flag) {
		data_.n.i64 = i64;
		if (i64 >= 0) {
			flags_ |= knumberuint64flag;
			if (!(i64 & 0xffffffff00000000ll))
				flags_ |= kuintflag;
			if (!(i64 & 0xffffffff80000000ll))
				flags_ |= kintflag;
		}
		else if (i64 >= -2147483648ll)
			flags_ |= kintflag;
	}

	//! constructor for uint64_t value.
	genericvalue(uint64_t u64) : flags_(knumberuint64flag) {
		data_.n.u64 = u64;
		if (!(u64 & 0x8000000000000000ull))
			flags_ |= kint64flag;
		if (!(u64 & 0xffffffff00000000ull))
			flags_ |= kuintflag;
		if (!(u64 & 0xffffffff80000000ull))
			flags_ |= kintflag;
	}

	//! constructor for double value.
	genericvalue(double d) : flags_(knumberdoubleflag) { data_.n.d = d; }

	//! constructor for constant string (i.e. do not make a copy of string)
	genericvalue(const ch* s, sizetype length) { 
		rapidjson_assert(s != null);
		flags_ = kconststringflag;
		data_.s.str = s;
		data_.s.length = length;
	}

	//! constructor for constant string (i.e. do not make a copy of string)
	genericvalue(const ch* s) { setstringraw(s, internal::strlen(s)); }

	//! constructor for copy-string (i.e. do make a copy of string)
	genericvalue(const ch* s, sizetype length, allocator& allocator) { setstringraw(s, length, allocator); }

	//! constructor for copy-string (i.e. do make a copy of string)
	genericvalue(const ch*s, allocator& allocator) { setstringraw(s, internal::strlen(s), allocator); }

	//! destructor.
	/*! need to destruct elements of array, members of object, or copy-string.
	*/
	~genericvalue() {
		if (allocator::kneedfree) {	// shortcut by allocator's trait
			switch(flags_) {
			case karrayflag:
				for (genericvalue* v = data_.a.elements; v != data_.a.elements + data_.a.size; ++v)
					v->~genericvalue();
				allocator::free(data_.a.elements);
				break;

			case kobjectflag:
				for (member* m = data_.o.members; m != data_.o.members + data_.o.size; ++m) {
					m->name.~genericvalue();
					m->value.~genericvalue();
				}
				allocator::free(data_.o.members);
				break;

			case kcopystringflag:
				allocator::free(const_cast<ch*>(data_.s.str));
				break;
			}
		}
	}

	//@}

	//!@name assignment operators
	//@{

	//! assignment with move semantics.
	/*! \param rhs source of the assignment. it will become a null value after assignment.
	*/
	genericvalue& operator=(genericvalue& rhs) {
		rapidjson_assert(this != &rhs);
		this->~genericvalue();
		memcpy(this, &rhs, sizeof(genericvalue));
		rhs.flags_ = knullflag;
		return *this;
	}

	//! assignment with primitive types.
	/*! \tparam t either type, int, unsigned, int64_t, uint64_t, const ch*
		\param value the value to be assigned.
	*/
	template <typename t>
	genericvalue& operator=(t value) {
		this->~genericvalue();
		new (this) genericvalue(value);
		return *this;
	}
	//@}

	//!@name type
	//@{

	type gettype()	const { return static_cast<type>(flags_ & ktypemask); }
	bool isnull()	const { return flags_ == knullflag; }
	bool isfalse()	const { return flags_ == kfalseflag; }
	bool istrue()	const { return flags_ == ktrueflag; }
	bool isbool()	const { return (flags_ & kboolflag) != 0; }
	bool isobject()	const { return flags_ == kobjectflag; }
	bool isarray()	const { return flags_ == karrayflag; }
	bool isnumber() const { return (flags_ & knumberflag) != 0; }
	bool isint()	const { return (flags_ & kintflag) != 0; }
	bool isuint()	const { return (flags_ & kuintflag) != 0; }
	bool isint64()	const { return (flags_ & kint64flag) != 0; }
	bool isuint64()	const { return (flags_ & kuint64flag) != 0; }
	bool isdouble() const { return (flags_ & kdoubleflag) != 0; }
	bool isstring() const { return (flags_ & kstringflag) != 0; }

	//@}

	//!@name null
	//@{

	genericvalue& setnull() { this->~genericvalue(); new (this) genericvalue(); return *this; }

	//@}

	//!@name bool
	//@{

	bool getbool() const { rapidjson_assert(isbool()); return flags_ == ktrueflag; }
	genericvalue& setbool(bool b) { this->~genericvalue(); new (this) genericvalue(b); return *this; }

	//@}

	//!@name object
	//@{

	//! set this value as an empty object.
	genericvalue& setobject() { this->~genericvalue(); new (this) genericvalue(kobjecttype); return *this; }

	//! get the value associated with the object's name.
	genericvalue& operator[](const ch* name) {
		if (member* member = findmember(name))
			return member->value;
		else {
			static genericvalue nullvalue;
			return nullvalue;
		}
	}
	const genericvalue& operator[](const ch* name) const { return const_cast<genericvalue&>(*this)[name]; }

	//! member iterators.
	constmemberiterator memberbegin() const	{ rapidjson_assert(isobject()); return data_.o.members; }
	constmemberiterator memberend()	const	{ rapidjson_assert(isobject()); return data_.o.members + data_.o.size; }
	memberiterator memberbegin()			{ rapidjson_assert(isobject()); return data_.o.members; }
	memberiterator memberend()				{ rapidjson_assert(isobject()); return data_.o.members + data_.o.size; }

	//! check whether a member exists in the object.
	bool hasmember(const ch* name) const { return findmember(name) != 0; }

	//! add a member (name-value pair) to the object.
	/*! \param name a string value as name of member.
		\param value value of any type.
	    \param allocator allocator for reallocating memory.
	    \return the value itself for fluent api.
	    \note the ownership of name and value will be transfered to this object if success.
	*/
	genericvalue& addmember(genericvalue& name, genericvalue& value, allocator& allocator) {
		rapidjson_assert(isobject());
		rapidjson_assert(name.isstring());
		object& o = data_.o;
		if (o.size >= o.capacity) {
			if (o.capacity == 0) {
				o.capacity = kdefaultobjectcapacity;
				o.members = (member*)allocator.malloc(o.capacity * sizeof(member));
			}
			else {
				sizetype oldcapacity = o.capacity;
				o.capacity *= 2;
				o.members = (member*)allocator.realloc(o.members, oldcapacity * sizeof(member), o.capacity * sizeof(member));
			}
		}
		o.members[o.size].name.rawassign(name);
		o.members[o.size].value.rawassign(value);
		o.size++;
		return *this;
	}

	genericvalue& addmember(const ch* name, allocator& nameallocator, genericvalue& value, allocator& allocator) {
		genericvalue n(name, internal::strlen(name), nameallocator);
		return addmember(n, value, allocator);
	}

	genericvalue& addmember(const ch* name, genericvalue& value, allocator& allocator) {
		genericvalue n(name, internal::strlen(name));
		return addmember(n, value, allocator);
	}

	template <typename t>
	genericvalue& addmember(const ch* name, t value, allocator& allocator) {
		genericvalue n(name, internal::strlen(name));
		genericvalue v(value);
		return addmember(n, v, allocator);
	}

	//! remove a member in object by its name.
	/*! \param name name of member to be removed.
	    \return whether the member existed.
	    \note removing member is implemented by moving the last member. so the ordering of members is changed.
	*/
	bool removemember(const ch* name) {
		rapidjson_assert(isobject());
		if (member* m = findmember(name)) {
			rapidjson_assert(data_.o.size > 0);
			rapidjson_assert(data_.o.members != 0);

			member* last = data_.o.members + (data_.o.size - 1);
			if (data_.o.size > 1 && m != last) {
				// move the last one to this place
				m->name = last->name;
				m->value = last->value;
			}
			else {
				// only one left, just destroy
				m->name.~genericvalue();
				m->value.~genericvalue();
			}
			--data_.o.size;
			return true;
		}
		return false;
	}

	//@}

	//!@name array
	//@{

	//! set this value as an empty array.
	genericvalue& setarray() {	this->~genericvalue(); new (this) genericvalue(karraytype); return *this; }

	//! get the number of elements in array.
	sizetype size() const { rapidjson_assert(isarray()); return data_.a.size; }

	//! get the capacity of array.
	sizetype capacity() const { rapidjson_assert(isarray()); return data_.a.capacity; }

	//! check whether the array is empty.
	bool empty() const { rapidjson_assert(isarray()); return data_.a.size == 0; }

	//! remove all elements in the array.
	/*! this function do not deallocate memory in the array, i.e. the capacity is unchanged.
	*/
	void clear() {
		rapidjson_assert(isarray()); 
		for (sizetype i = 0; i < data_.a.size; ++i)
			data_.a.elements[i].~genericvalue();
		data_.a.size = 0;
	}

	//! get an element from array by index.
	/*! \param index zero-based index of element.
		\note
\code
value a(karraytype);
a.pushback(123);
int x = a[0].getint();				// error: operator[ is ambiguous, as 0 also mean a null pointer of const char* type.
int y = a[sizetype(0)].getint();	// cast to sizetype will work.
int z = a[0u].getint();				// this works too.
\endcode
	*/
	genericvalue& operator[](sizetype index) {
		rapidjson_assert(isarray());
		rapidjson_assert(index < data_.a.size);
		return data_.a.elements[index];
	}
	const genericvalue& operator[](sizetype index) const { return const_cast<genericvalue&>(*this)[index]; }

	//! element iterator
	valueiterator begin() { rapidjson_assert(isarray()); return data_.a.elements; }
	valueiterator end() { rapidjson_assert(isarray()); return data_.a.elements + data_.a.size; }
	constvalueiterator begin() const { return const_cast<genericvalue&>(*this).begin(); }
	constvalueiterator end() const { return const_cast<genericvalue&>(*this).end(); }

	//! request the array to have enough capacity to store elements.
	/*! \param newcapacity	the capacity that the array at least need to have.
		\param allocator	the allocator for allocating memory. it must be the same one use previously.
		\return the value itself for fluent api.
	*/
	genericvalue& reserve(sizetype newcapacity, allocator &allocator) {
		rapidjson_assert(isarray());
		if (newcapacity > data_.a.capacity) {
			data_.a.elements = (genericvalue*)allocator.realloc(data_.a.elements, data_.a.capacity * sizeof(genericvalue), newcapacity * sizeof(genericvalue));
			data_.a.capacity = newcapacity;
		}
		return *this;
	}

	//! append a value at the end of the array.
	/*! \param value		the value to be appended.
	    \param allocator	the allocator for allocating memory. it must be the same one use previously.
	    \return the value itself for fluent api.
	    \note the ownership of the value will be transfered to this object if success.
	    \note if the number of elements to be appended is known, calls reserve() once first may be more efficient.
	*/
	genericvalue& pushback(genericvalue& value, allocator& allocator) {
		rapidjson_assert(isarray());
		if (data_.a.size >= data_.a.capacity)
			reserve(data_.a.capacity == 0 ? kdefaultarraycapacity : data_.a.capacity * 2, allocator);
		data_.a.elements[data_.a.size++].rawassign(value);
		return *this;
	}

	template <typename t>
	genericvalue& pushback(t value, allocator& allocator) {
		genericvalue v(value);
		return pushback(v, allocator);
	}

	//! remove the last element in the array.
	genericvalue& popback() {
		rapidjson_assert(isarray());
		rapidjson_assert(!empty());
		data_.a.elements[--data_.a.size].~genericvalue();
		return *this;
	}
	//@}

	//!@name number
	//@{

	int getint() const			{ rapidjson_assert(flags_ & kintflag);   return data_.n.i.i;   }
	unsigned getuint() const	{ rapidjson_assert(flags_ & kuintflag);  return data_.n.u.u;   }
	int64_t getint64() const	{ rapidjson_assert(flags_ & kint64flag); return data_.n.i64; }
	uint64_t getuint64() const	{ rapidjson_assert(flags_ & kuint64flag); return data_.n.u64; }

	double getdouble() const {
		rapidjson_assert(isnumber());
		if ((flags_ & kdoubleflag) != 0)				return data_.n.d;	// exact type, no conversion.
		if ((flags_ & kintflag) != 0)					return data_.n.i.i;	// int -> double
		if ((flags_ & kuintflag) != 0)					return data_.n.u.u;	// unsigned -> double
		if ((flags_ & kint64flag) != 0)					return (double)data_.n.i64; // int64_t -> double (may lose precision)
		rapidjson_assert((flags_ & kuint64flag) != 0);	return (double)data_.n.u64;	// uint64_t -> double (may lose precision)
	}

	genericvalue& setint(int i)				{ this->~genericvalue(); new (this) genericvalue(i);	return *this; }
	genericvalue& setuint(unsigned u)		{ this->~genericvalue(); new (this) genericvalue(u);	return *this; }
	genericvalue& setint64(int64_t i64)		{ this->~genericvalue(); new (this) genericvalue(i64);	return *this; }
	genericvalue& setuint64(uint64_t u64)	{ this->~genericvalue(); new (this) genericvalue(u64);	return *this; }
	genericvalue& setdouble(double d)		{ this->~genericvalue(); new (this) genericvalue(d);	return *this; }

	//@}

	//!@name string
	//@{

	const ch* getstring() const { rapidjson_assert(isstring()); return data_.s.str; }

	//! get the length of string.
	/*! since rapidjson permits "\u0000" in the json string, strlen(v.getstring()) may not equal to v.getstringlength().
	*/
	sizetype getstringlength() const { rapidjson_assert(isstring()); return data_.s.length; }

	//! set this value as a string without copying source string.
	/*! this version has better performance with supplied length, and also support string containing null character.
		\param s source string pointer. 
		\param length the length of source string, excluding the trailing null terminator.
		\return the value itself for fluent api.
	*/
	genericvalue& setstring(const ch* s, sizetype length) { this->~genericvalue(); setstringraw(s, length); return *this; }

	//! set this value as a string without copying source string.
	/*! \param s source string pointer. 
		\return the value itself for fluent api.
	*/
	genericvalue& setstring(const ch* s) { return setstring(s, internal::strlen(s)); }

	//! set this value as a string by copying from source string.
	/*! this version has better performance with supplied length, and also support string containing null character.
		\param s source string. 
		\param length the length of source string, excluding the trailing null terminator.
		\param allocator allocator for allocating copied buffer. commonly use document.getallocator().
		\return the value itself for fluent api.
	*/
	genericvalue& setstring(const ch* s, sizetype length, allocator& allocator) { this->~genericvalue(); setstringraw(s, length, allocator); return *this; }

	//! set this value as a string by copying from source string.
	/*!	\param s source string. 
		\param allocator allocator for allocating copied buffer. commonly use document.getallocator().
		\return the value itself for fluent api.
	*/
	genericvalue& setstring(const ch* s, allocator& allocator) {	setstring(s, internal::strlen(s), allocator); return *this; }

	//@}

	//! generate events of this value to a handler.
	/*! this function adopts the gof visitor pattern.
		typical usage is to output this json value as json text via writer, which is a handler.
		it can also be used to deep clone this value via genericdocument, which is also a handler.
		\tparam handler type of handler.
		\param handler an object implementing concept handler.
	*/
	template <typename handler>
	const genericvalue& accept(handler& handler) const {
		switch(gettype()) {
		case knulltype:		handler.null(); break;
		case kfalsetype:	handler.bool(false); break;
		case ktruetype:		handler.bool(true); break;

		case kobjecttype:
			handler.startobject();
			for (member* m = data_.o.members; m != data_.o.members + data_.o.size; ++m) {
				handler.string(m->name.data_.s.str, m->name.data_.s.length, false);
				m->value.accept(handler);
			}
			handler.endobject(data_.o.size);
			break;

		case karraytype:
			handler.startarray();
			for (genericvalue* v = data_.a.elements; v != data_.a.elements + data_.a.size; ++v)
				v->accept(handler);
			handler.endarray(data_.a.size);
			break;

		case kstringtype:
			handler.string(data_.s.str, data_.s.length, false);
			break;

		case knumbertype:
			if (isint())			handler.int(data_.n.i.i);
			else if (isuint())		handler.uint(data_.n.u.u);
			else if (isint64())		handler.int64(data_.n.i64);
			else if (isuint64())	handler.uint64(data_.n.u64);
			else					handler.double(data_.n.d);
			break;
		}
		return *this;
	}

private:
	template <typename, typename>
	friend class genericdocument;

	enum {
		kboolflag = 0x100,
		knumberflag = 0x200,
		kintflag = 0x400,
		kuintflag = 0x800,
		kint64flag = 0x1000,
		kuint64flag = 0x2000,
		kdoubleflag = 0x4000,
		kstringflag = 0x100000,
		kcopyflag = 0x200000,

		// initial flags of different types.
		knullflag = knulltype,
		ktrueflag = ktruetype | kboolflag,
		kfalseflag = kfalsetype | kboolflag,
		knumberintflag = knumbertype | knumberflag | kintflag | kint64flag,
		knumberuintflag = knumbertype | knumberflag | kuintflag | kuint64flag | kint64flag,
		knumberint64flag = knumbertype | knumberflag | kint64flag,
		knumberuint64flag = knumbertype | knumberflag | kuint64flag,
		knumberdoubleflag = knumbertype | knumberflag | kdoubleflag,
		kconststringflag = kstringtype | kstringflag,
		kcopystringflag = kstringtype | kstringflag | kcopyflag,
		kobjectflag = kobjecttype,
		karrayflag = karraytype,

		ktypemask = 0xff	// bitwise-and with mask of 0xff can be optimized by compiler
	};

	static const sizetype kdefaultarraycapacity = 16;
	static const sizetype kdefaultobjectcapacity = 16;

	struct string {
		const ch* str;
		sizetype length;
		unsigned hashcode;	//!< reserved
	};	// 12 bytes in 32-bit mode, 16 bytes in 64-bit mode

	// by using proper binary layout, retrieval of different integer types do not need conversions.
	union number {
#if rapidjson_endian == rapidjson_littleendian
		struct i {
			int i;
			char padding[4];
		}i;
		struct u {
			unsigned u;
			char padding2[4];
		}u;
#else
		struct i {
			char padding[4];
			int i;
		}i;
		struct u {
			char padding2[4];
			unsigned u;
		}u;
#endif
		int64_t i64;
		uint64_t u64;
		double d;
	};	// 8 bytes

	struct object {
		member* members;
		sizetype size;
		sizetype capacity;
	};	// 12 bytes in 32-bit mode, 16 bytes in 64-bit mode

	struct array {
		genericvalue<encoding, allocator>* elements;
		sizetype size;
		sizetype capacity;
	};	// 12 bytes in 32-bit mode, 16 bytes in 64-bit mode

	union data {
		string s;
		number n;
		object o;
		array a;
	};	// 12 bytes in 32-bit mode, 16 bytes in 64-bit mode

	//! find member by name.
	member* findmember(const ch* name) {
		rapidjson_assert(name);
		rapidjson_assert(isobject());

		sizetype length = internal::strlen(name);

		object& o = data_.o;
		for (member* member = o.members; member != data_.o.members + data_.o.size; ++member)
			if (length == member->name.data_.s.length && memcmp(member->name.data_.s.str, name, length * sizeof(ch)) == 0)
				return member;

		return 0;
	}
	const member* findmember(const ch* name) const { return const_cast<genericvalue&>(*this).findmember(name); }

	// initialize this value as array with initial data, without calling destructor.
	void setarrayraw(genericvalue* values, sizetype count, allocator& alloctaor) {
		flags_ = karrayflag;
		data_.a.elements = (genericvalue*)alloctaor.malloc(count * sizeof(genericvalue));
		memcpy(data_.a.elements, values, count * sizeof(genericvalue));
		data_.a.size = data_.a.capacity = count;
	}

	//! initialize this value as object with initial data, without calling destructor.
	void setobjectraw(member* members, sizetype count, allocator& alloctaor) {
		flags_ = kobjectflag;
		data_.o.members = (member*)alloctaor.malloc(count * sizeof(member));
		memcpy(data_.o.members, members, count * sizeof(member));
		data_.o.size = data_.o.capacity = count;
	}

	//! initialize this value as constant string, without calling destructor.
	void setstringraw(const ch* s, sizetype length) {
		rapidjson_assert(s != null);
		flags_ = kconststringflag;
		data_.s.str = s;
		data_.s.length = length;
	}

	//! initialize this value as copy string with initial data, without calling destructor.
	void setstringraw(const ch* s, sizetype length, allocator& allocator) {
		rapidjson_assert(s != null);
		flags_ = kcopystringflag;
		data_.s.str = (ch *)allocator.malloc((length + 1) * sizeof(ch));
		data_.s.length = length;
		memcpy(const_cast<ch*>(data_.s.str), s, length * sizeof(ch));
		const_cast<ch*>(data_.s.str)[length] = '\0';
	}

	//! assignment without calling destructor
	void rawassign(genericvalue& rhs) {
		memcpy(this, &rhs, sizeof(genericvalue));
		rhs.flags_ = knullflag;
	}

	data data_;
	unsigned flags_;
};
#pragma pack (pop)

//! value with utf8 encoding.
typedef genericvalue<utf8<> > value;

///////////////////////////////////////////////////////////////////////////////
// genericdocument 

//! a document for parsing json text as dom.
/*!
	\implements handler
	\tparam encoding encoding for both parsing and string storage.
	\tparam alloactor allocator for allocating memory for the dom, and the stack during parsing.
*/
template <typename encoding, typename allocator = memorypoolallocator<> >
class genericdocument : public genericvalue<encoding, allocator> {
public:
	typedef typename encoding::ch ch;						//!< character type derived from encoding.
	typedef genericvalue<encoding, allocator> valuetype;	//!< value type of the document.
	typedef allocator allocatortype;						//!< allocator type from template parameter.

	//! constructor
	/*! \param allocator		optional allocator for allocating stack memory.
		\param stackcapacity	initial capacity of stack in bytes.
	*/
	genericdocument(allocator* allocator = 0, size_t stackcapacity = kdefaultstackcapacity) : stack_(allocator, stackcapacity), parseerror_(0), erroroffset_(0) {}

	//! parse json text from an input stream.
	/*! \tparam parseflags combination of parseflag.
		\param stream input stream to be parsed.
		\return the document itself for fluent api.
	*/
	template <unsigned parseflags, typename stream>
	genericdocument& parsestream(stream& stream) {
		valuetype::setnull(); // remove existing root if exist
		genericreader<encoding, allocator> reader;
		if (reader.template parse<parseflags>(stream, *this)) {
			rapidjson_assert(stack_.getsize() == sizeof(valuetype)); // got one and only one root object
			this->rawassign(*stack_.template pop<valuetype>(1));	// add this-> to prevent issue 13.
			parseerror_ = 0;
			erroroffset_ = 0;
		}
		else {
			parseerror_ = reader.getparseerror();
			erroroffset_ = reader.geterroroffset();
			clearstack();
		}
		return *this;
	}

	//! parse json text from a mutable string.
	/*! \tparam parseflags combination of parseflag.
		\param str mutable zero-terminated string to be parsed.
		\return the document itself for fluent api.
	*/
	template <unsigned parseflags>
	genericdocument& parseinsitu(ch* str) {
		genericinsitustringstream<encoding> s(str);
		return parsestream<parseflags | kparseinsituflag>(s);
	}

	//! parse json text from a read-only string.
	/*! \tparam parseflags combination of parseflag (must not contain kparseinsituflag).
		\param str read-only zero-terminated string to be parsed.
	*/
	template <unsigned parseflags>
	genericdocument& parse(const ch* str) {
		rapidjson_assert(!(parseflags & kparseinsituflag));
		genericstringstream<encoding> s(str);
		return parsestream<parseflags>(s);
	}

	//! whether a parse error was occured in the last parsing.
	bool hasparseerror() const { return parseerror_ != 0; }

	//! get the message of parsing error.
	const char* getparseerror() const { return parseerror_; }

	//! get the offset in character of the parsing error.
	size_t geterroroffset() const { return erroroffset_; }

	//! get the allocator of this document.
	allocator& getallocator() {	return stack_.getallocator(); }

	//! get the capacity of stack in bytes.
	size_t getstackcapacity() const { return stack_.getcapacity(); }

private:
	// prohibit assignment
	genericdocument& operator=(const genericdocument&);

	friend class genericreader<encoding, allocator>;	// for reader to call the following private handler functions

	// implementation of handler
	void null()	{ new (stack_.template push<valuetype>()) valuetype(); }
	void bool(bool b) { new (stack_.template push<valuetype>()) valuetype(b); }
	void int(int i) { new (stack_.template push<valuetype>()) valuetype(i); }
	void uint(unsigned i) { new (stack_.template push<valuetype>()) valuetype(i); }
	void int64(int64_t i) { new (stack_.template push<valuetype>()) valuetype(i); }
	void uint64(uint64_t i) { new (stack_.template push<valuetype>()) valuetype(i); }
	void double(double d) { new (stack_.template push<valuetype>()) valuetype(d); }

	void string(const ch* str, sizetype length, bool copy) { 
		if (copy) 
			new (stack_.template push<valuetype>()) valuetype(str, length, getallocator());
		else
			new (stack_.template push<valuetype>()) valuetype(str, length);
	}

	void startobject() { new (stack_.template push<valuetype>()) valuetype(kobjecttype); }
	
	void endobject(sizetype membercount) {
		typename valuetype::member* members = stack_.template pop<typename valuetype::member>(membercount);
		stack_.template top<valuetype>()->setobjectraw(members, (sizetype)membercount, getallocator());
	}

	void startarray() { new (stack_.template push<valuetype>()) valuetype(karraytype); }
	
	void endarray(sizetype elementcount) {
		valuetype* elements = stack_.template pop<valuetype>(elementcount);
		stack_.template top<valuetype>()->setarrayraw(elements, elementcount, getallocator());
	}

	void clearstack() {
		if (allocator::kneedfree)
			while (stack_.getsize() > 0)	// here assumes all elements in stack array are genericvalue (member is actually 2 genericvalue objects)
				(stack_.template pop<valuetype>(1))->~valuetype();
		else
			stack_.clear();
	}

	static const size_t kdefaultstackcapacity = 1024;
	internal::stack<allocator> stack_;
	const char* parseerror_;
	size_t erroroffset_;
};

typedef genericdocument<utf8<> > document;

} // namespace rapidjson

#ifdef _msc_ver
#pragma warning(pop)
#endif

#endif // rapidjson_document_h_
