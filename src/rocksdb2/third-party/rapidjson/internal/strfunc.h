#ifndef rapidjson_internal_strfunc_h_
#define rapidjson_internal_strfunc_h_

namespace rapidjson {
namespace internal {

//! custom strlen() which works on different character types.
/*!	\tparam ch character type (e.g. char, wchar_t, short)
	\param s null-terminated input string.
	\return number of characters in the string. 
	\note this has the same semantics as strlen(), the return value is not number of unicode codepoints.
*/
template <typename ch>
inline sizetype strlen(const ch* s) {
	const ch* p = s;
	while (*p != '\0')
		++p;
	return sizetype(p - s);
}

} // namespace internal
} // namespace rapidjson

#endif // rapidjson_internal_strfunc_h_
