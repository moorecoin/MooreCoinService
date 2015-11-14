#ifndef rapidjson_stringbuffer_h_
#define rapidjson_stringbuffer_h_

#include "rapidjson.h"
#include "internal/stack.h"

namespace rapidjson {

//! represents an in-memory output stream.
/*!
	\tparam encoding encoding of the stream.
	\tparam allocator type for allocating memory buffer.
	\implements stream
*/
template <typename encoding, typename allocator = crtallocator>
struct genericstringbuffer {
	typedef typename encoding::ch ch;

	genericstringbuffer(allocator* allocator = 0, size_t capacity = kdefaultcapacity) : stack_(allocator, capacity) {}

	void put(ch c) { *stack_.template push<ch>() = c; }

	void clear() { stack_.clear(); }

	const char* getstring() const {
		// push and pop a null terminator. this is safe.
		*stack_.template push<ch>() = '\0';
		stack_.template pop<ch>(1);

		return stack_.template bottom<ch>();
	}

	size_t size() const { return stack_.getsize(); }

	static const size_t kdefaultcapacity = 256;
	mutable internal::stack<allocator> stack_;
};

typedef genericstringbuffer<utf8<> > stringbuffer;

//! implement specialized version of putn() with memset() for better performance.
template<>
inline void putn(genericstringbuffer<utf8<> >& stream, char c, size_t n) {
	memset(stream.stack_.push<char>(n), c, n * sizeof(c));
}

} // namespace rapidjson

#endif // rapidjson_stringbuffer_h_
