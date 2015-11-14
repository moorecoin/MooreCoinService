#ifndef rapidjson_rapidjson_h_
#define rapidjson_rapidjson_h_

// copyright (c) 2011-2012 milo yip (miloyip@gmail.com)
// version 0.11

#include <cstdlib>	// malloc(), realloc(), free()
#include <cstring>	// memcpy()

///////////////////////////////////////////////////////////////////////////////
// rapidjson_no_int64define

// here defines int64_t and uint64_t types in global namespace.
// if user have their own definition, can define rapidjson_no_int64define to disable this.
#ifndef rapidjson_no_int64define
#ifdef _msc_ver
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <inttypes.h>
#endif
#endif // rapidjson_no_int64typedef

///////////////////////////////////////////////////////////////////////////////
// rapidjson_endian
#define rapidjson_littleendian	0	//!< little endian machine
#define rapidjson_bigendian		1	//!< big endian machine

//! endianness of the machine.
/*!	gcc provided macro for detecting endianness of the target machine. but other
	compilers may not have this. user can define rapidjson_endian to either
	rapidjson_littleendian or rapidjson_bigendian.
*/
#ifndef rapidjson_endian
#ifdef __byte_order__
#if __byte_order__ == __order_little_endian__
#define rapidjson_endian rapidjson_littleendian
#else
#define rapidjson_endian rapidjson_bigendian
#endif // __byte_order__
#else
#define rapidjson_endian rapidjson_littleendian	// assumes little endian otherwise.
#endif
#endif // rapidjson_endian

///////////////////////////////////////////////////////////////////////////////
// rapidjson_sse2/rapidjson_sse42/rapidjson_simd

// enable sse2 optimization.
//#define rapidjson_sse2

// enable sse4.2 optimization.
//#define rapidjson_sse42

#if defined(rapidjson_sse2) || defined(rapidjson_sse42)
#define rapidjson_simd
#endif

///////////////////////////////////////////////////////////////////////////////
// rapidjson_no_sizetypedefine

#ifndef rapidjson_no_sizetypedefine
namespace rapidjson {
//! use 32-bit array/string indices even for 64-bit platform, instead of using size_t.
/*! user may override the sizetype by defining rapidjson_no_sizetypedefine.
*/
typedef unsigned sizetype;
} // namespace rapidjson
#endif

///////////////////////////////////////////////////////////////////////////////
// rapidjson_assert

//! assertion.
/*! by default, rapidjson uses c assert() for assertion.
	user can override it by defining rapidjson_assert(x) macro.
*/
#ifndef rapidjson_assert
#include <cassert>
#define rapidjson_assert(x) assert(x)
#endif // rapidjson_assert

///////////////////////////////////////////////////////////////////////////////
// helpers

#define rapidjson_multilinemacro_begin do {  
#define rapidjson_multilinemacro_end \
} while((void)0, 0)

namespace rapidjson {

///////////////////////////////////////////////////////////////////////////////
// allocator

/*! \class rapidjson::allocator
	\brief concept for allocating, resizing and freeing memory block.
	
	note that malloc() and realloc() are non-static but free() is static.
	
	so if an allocator need to support free(), it needs to put its pointer in 
	the header of memory block.

\code
concept allocator {
	static const bool kneedfree;	//!< whether this allocator needs to call free().

	// allocate a memory block.
	// \param size of the memory block in bytes.
	// \returns pointer to the memory block.
	void* malloc(size_t size);

	// resize a memory block.
	// \param originalptr the pointer to current memory block. null pointer is permitted.
	// \param originalsize the current size in bytes. (design issue: since some allocator may not book-keep this, explicitly pass to it can save memory.)
	// \param newsize the new size in bytes.
	void* realloc(void* originalptr, size_t originalsize, size_t newsize);

	// free a memory block.
	// \param pointer to the memory block. null pointer is permitted.
	static void free(void *ptr);
};
\endcode
*/

///////////////////////////////////////////////////////////////////////////////
// crtallocator

//! c-runtime library allocator.
/*! this class is just wrapper for standard c library memory routines.
	\implements allocator
*/
class crtallocator {
public:
	static const bool kneedfree = true;
	void* malloc(size_t size) { return malloc(size); }
	void* realloc(void* originalptr, size_t originalsize, size_t newsize) { (void)originalsize; return realloc(originalptr, newsize); }
	static void free(void *ptr) { free(ptr); }
};

///////////////////////////////////////////////////////////////////////////////
// memorypoolallocator

//! default memory allocator used by the parser and dom.
/*! this allocator allocate memory blocks from pre-allocated memory chunks. 

    it does not free memory blocks. and realloc() only allocate new memory.

    the memory chunks are allocated by baseallocator, which is crtallocator by default.

    user may also supply a buffer as the first chunk.

    if the user-buffer is full then additional chunks are allocated by baseallocator.

    the user-buffer is not deallocated by this allocator.

    \tparam baseallocator the allocator type for allocating memory chunks. default is crtallocator.
	\implements allocator
*/
template <typename baseallocator = crtallocator>
class memorypoolallocator {
public:
	static const bool kneedfree = false;	//!< tell users that no need to call free() with this allocator. (concept allocator)

	//! constructor with chunksize.
	/*! \param chunksize the size of memory chunk. the default is kdefaultchunksize.
		\param baseallocator the allocator for allocating memory chunks.
	*/
	memorypoolallocator(size_t chunksize = kdefaultchunkcapacity, baseallocator* baseallocator = 0) : 
		chunkhead_(0), chunk_capacity_(chunksize), userbuffer_(0), baseallocator_(baseallocator), ownbaseallocator_(0)
	{
		if (!baseallocator_)
			ownbaseallocator_ = baseallocator_ = new baseallocator();
		addchunk(chunk_capacity_);
	}

	//! constructor with user-supplied buffer.
	/*! the user buffer will be used firstly. when it is full, memory pool allocates new chunk with chunk size.

		the user buffer will not be deallocated when this allocator is destructed.

		\param buffer user supplied buffer.
		\param size size of the buffer in bytes. it must at least larger than sizeof(chunkheader).
		\param chunksize the size of memory chunk. the default is kdefaultchunksize.
		\param baseallocator the allocator for allocating memory chunks.
	*/
	memorypoolallocator(char *buffer, size_t size, size_t chunksize = kdefaultchunkcapacity, baseallocator* baseallocator = 0) :
		chunkhead_(0), chunk_capacity_(chunksize), userbuffer_(buffer), baseallocator_(baseallocator), ownbaseallocator_(0)
	{
		rapidjson_assert(buffer != 0);
		rapidjson_assert(size > sizeof(chunkheader));
		chunkhead_ = (chunkheader*)buffer;
		chunkhead_->capacity = size - sizeof(chunkheader);
		chunkhead_->size = 0;
		chunkhead_->next = 0;
	}

	//! destructor.
	/*! this deallocates all memory chunks, excluding the user-supplied buffer.
	*/
	~memorypoolallocator() {
		clear();
		delete ownbaseallocator_;
	}

	//! deallocates all memory chunks, excluding the user-supplied buffer.
	void clear() {
		while(chunkhead_ != 0 && chunkhead_ != (chunkheader *)userbuffer_) {
			chunkheader* next = chunkhead_->next;
			baseallocator_->free(chunkhead_);
			chunkhead_ = next;
		}
	}

	//! computes the total capacity of allocated memory chunks.
	/*! \return total capacity in bytes.
	*/
	size_t capacity() {
		size_t capacity = 0;
		for (chunkheader* c = chunkhead_; c != 0; c = c->next)
			capacity += c->capacity;
		return capacity;
	}

	//! computes the memory blocks allocated.
	/*! \return total used bytes.
	*/
	size_t size() {
		size_t size = 0;
		for (chunkheader* c = chunkhead_; c != 0; c = c->next)
			size += c->size;
		return size;
	}

	//! allocates a memory block. (concept allocator)
	void* malloc(size_t size) {
		size = (size + 3) & ~3;	// force aligning size to 4

		if (chunkhead_->size + size > chunkhead_->capacity)
			addchunk(chunk_capacity_ > size ? chunk_capacity_ : size);

		char *buffer = (char *)(chunkhead_ + 1) + chunkhead_->size;
		rapidjson_assert(((uintptr_t)buffer & 3) == 0);	// returned buffer is aligned to 4
		chunkhead_->size += size;

		return buffer;
	}

	//! resizes a memory block (concept allocator)
	void* realloc(void* originalptr, size_t originalsize, size_t newsize) {
		if (originalptr == 0)
			return malloc(newsize);

		// do not shrink if new size is smaller than original
		if (originalsize >= newsize)
			return originalptr;

		// simply expand it if it is the last allocation and there is sufficient space
		if (originalptr == (char *)(chunkhead_ + 1) + chunkhead_->size - originalsize) {
			size_t increment = newsize - originalsize;
			increment = (increment + 3) & ~3;	// force aligning size to 4
			if (chunkhead_->size + increment <= chunkhead_->capacity) {
				chunkhead_->size += increment;
				rapidjson_assert(((uintptr_t)originalptr & 3) == 0);	// returned buffer is aligned to 4
				return originalptr;
			}
		}

		// realloc process: allocate and copy memory, do not free original buffer.
		void* newbuffer = malloc(newsize);
		rapidjson_assert(newbuffer != 0);	// do not handle out-of-memory explicitly.
		return memcpy(newbuffer, originalptr, originalsize);
	}

	//! frees a memory block (concept allocator)
	static void free(void *) {} // do nothing

private:
	//! creates a new chunk.
	/*! \param capacity capacity of the chunk in bytes.
	*/
	void addchunk(size_t capacity) {
		chunkheader* chunk = (chunkheader*)baseallocator_->malloc(sizeof(chunkheader) + capacity);
		chunk->capacity = capacity;
		chunk->size = 0;
		chunk->next = chunkhead_;
		chunkhead_ =  chunk;
	}

	static const int kdefaultchunkcapacity = 64 * 1024; //!< default chunk capacity.

	//! chunk header for perpending to each chunk.
	/*! chunks are stored as a singly linked list.
	*/
	struct chunkheader {
		size_t capacity;	//!< capacity of the chunk in bytes (excluding the header itself).
		size_t size;		//!< current size of allocated memory in bytes.
		chunkheader *next;	//!< next chunk in the linked list.
	};

	chunkheader *chunkhead_;	//!< head of the chunk linked-list. only the head chunk serves allocation.
	size_t chunk_capacity_;		//!< the minimum capacity of chunk when they are allocated.
	char *userbuffer_;			//!< user supplied buffer.
	baseallocator* baseallocator_;	//!< base allocator for allocating memory chunks.
	baseallocator* ownbaseallocator_;	//!< base allocator created by this object.
};

///////////////////////////////////////////////////////////////////////////////
// encoding

/*! \class rapidjson::encoding
	\brief concept for encoding of unicode characters.

\code
concept encoding {
	typename ch;	//! type of character.

	//! \brief encode a unicode codepoint to a buffer.
	//! \param buffer pointer to destination buffer to store the result. it should have sufficient size of encoding one character.
	//! \param codepoint an unicode codepoint, ranging from 0x0 to 0x10ffff inclusively.
	//! \returns the pointer to the next character after the encoded data.
	static ch* encode(ch *buffer, unsigned codepoint);
};
\endcode
*/

///////////////////////////////////////////////////////////////////////////////
// utf8

//! utf-8 encoding.
/*! http://en.wikipedia.org/wiki/utf-8
	\tparam chartype type for storing 8-bit utf-8 data. default is char.
	\implements encoding
*/
template<typename chartype = char>
struct utf8 {
	typedef chartype ch;

	static ch* encode(ch *buffer, unsigned codepoint) {
		if (codepoint <= 0x7f) 
			*buffer++ = codepoint & 0xff;
		else if (codepoint <= 0x7ff) {
			*buffer++ = 0xc0 | ((codepoint >> 6) & 0xff);
			*buffer++ = 0x80 | ((codepoint & 0x3f));
		}
		else if (codepoint <= 0xffff) {
			*buffer++ = 0xe0 | ((codepoint >> 12) & 0xff);
			*buffer++ = 0x80 | ((codepoint >> 6) & 0x3f);
			*buffer++ = 0x80 | (codepoint & 0x3f);
		}
		else {
			rapidjson_assert(codepoint <= 0x10ffff);
			*buffer++ = 0xf0 | ((codepoint >> 18) & 0xff);
			*buffer++ = 0x80 | ((codepoint >> 12) & 0x3f);
			*buffer++ = 0x80 | ((codepoint >> 6) & 0x3f);
			*buffer++ = 0x80 | (codepoint & 0x3f);
		}
		return buffer;
	}
};

///////////////////////////////////////////////////////////////////////////////
// utf16

//! utf-16 encoding.
/*! http://en.wikipedia.org/wiki/utf-16
	\tparam chartype type for storing 16-bit utf-16 data. default is wchar_t. c++11 may use char16_t instead.
	\implements encoding
*/
template<typename chartype = wchar_t>
struct utf16 {
	typedef chartype ch;

	static ch* encode(ch* buffer, unsigned codepoint) {
		if (codepoint <= 0xffff) {
			rapidjson_assert(codepoint < 0xd800 || codepoint > 0xdfff); // code point itself cannot be surrogate pair 
			*buffer++ = static_cast<ch>(codepoint);
		}
		else {
			rapidjson_assert(codepoint <= 0x10ffff);
			unsigned v = codepoint - 0x10000;
			*buffer++ = static_cast<ch>((v >> 10) + 0xd800);
			*buffer++ = (v & 0x3ff) + 0xdc00;
		}
		return buffer;
	}
};

///////////////////////////////////////////////////////////////////////////////
// utf32

//! utf-32 encoding. 
/*! http://en.wikipedia.org/wiki/utf-32
	\tparam ch type for storing 32-bit utf-32 data. default is unsigned. c++11 may use char32_t instead.
	\implements encoding
*/
template<typename chartype = unsigned>
struct utf32 {
	typedef chartype ch;

	static ch *encode(ch* buffer, unsigned codepoint) {
		rapidjson_assert(codepoint <= 0x10ffff);
		*buffer++ = codepoint;
		return buffer;
	}
};

///////////////////////////////////////////////////////////////////////////////
//  stream

/*! \class rapidjson::stream
	\brief concept for reading and writing characters.

	for read-only stream, no need to implement putbegin(), put() and putend().

	for write-only stream, only need to implement put().

\code
concept stream {
	typename ch;	//!< character type of the stream.

	//! read the current character from stream without moving the read cursor.
	ch peek() const;

	//! read the current character from stream and moving the read cursor to next character.
	ch take();

	//! get the current read cursor.
	//! \return number of characters read from start.
	size_t tell();

	//! begin writing operation at the current read pointer.
	//! \return the begin writer pointer.
	ch* putbegin();

	//! write a character.
	void put(ch c);

	//! end the writing operation.
	//! \param begin the begin write pointer returned by putbegin().
	//! \return number of characters written.
	size_t putend(ch* begin);
}
\endcode
*/

//! put n copies of a character to a stream.
template<typename stream, typename ch>
inline void putn(stream& stream, ch c, size_t n) {
	for (size_t i = 0; i < n; i++)
		stream.put(c);
}

///////////////////////////////////////////////////////////////////////////////
// stringstream

//! read-only string stream.
/*! \implements stream
*/
template <typename encoding>
struct genericstringstream {
	typedef typename encoding::ch ch;

	genericstringstream(const ch *src) : src_(src), head_(src) {}

	ch peek() const { return *src_; }
	ch take() { return *src_++; }
	size_t tell() const { return src_ - head_; }

	ch* putbegin() { rapidjson_assert(false); return 0; }
	void put(ch) { rapidjson_assert(false); }
	size_t putend(ch*) { rapidjson_assert(false); return 0; }

	const ch* src_;		//!< current read position.
	const ch* head_;	//!< original head of the string.
};

typedef genericstringstream<utf8<> > stringstream;

///////////////////////////////////////////////////////////////////////////////
// insitustringstream

//! a read-write string stream.
/*! this string stream is particularly designed for in-situ parsing.
	\implements stream
*/
template <typename encoding>
struct genericinsitustringstream {
	typedef typename encoding::ch ch;

	genericinsitustringstream(ch *src) : src_(src), dst_(0), head_(src) {}

	// read
	ch peek() { return *src_; }
	ch take() { return *src_++; }
	size_t tell() { return src_ - head_; }

	// write
	ch* putbegin() { return dst_ = src_; }
	void put(ch c) { rapidjson_assert(dst_ != 0); *dst_++ = c; }
	size_t putend(ch* begin) { return dst_ - begin; }

	ch* src_;
	ch* dst_;
	ch* head_;
};

typedef genericinsitustringstream<utf8<> > insitustringstream;

///////////////////////////////////////////////////////////////////////////////
// type

//! type of json value
enum type {
	knulltype = 0,		//!< null
	kfalsetype = 1,		//!< false
	ktruetype = 2,		//!< true
	kobjecttype = 3,	//!< object
	karraytype = 4,		//!< array 
	kstringtype = 5,	//!< string
	knumbertype = 6,	//!< number
};

} // namespace rapidjson

#endif // rapidjson_rapidjson_h_
