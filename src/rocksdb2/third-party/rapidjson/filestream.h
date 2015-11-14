#ifndef rapidjson_filestream_h_
#define rapidjson_filestream_h_

#include <cstdio>

namespace rapidjson {

//! wrapper of c file stream for input or output.
/*!
	this simple wrapper does not check the validity of the stream.
	\implements stream
*/
class filestream {
public:
	typedef char ch;	//!< character type. only support char.

	filestream(file* fp) : fp_(fp), count_(0) { read(); }
	char peek() const { return current_; }
	char take() { char c = current_; read(); return c; }
	size_t tell() const { return count_; }
	void put(char c) { fputc(c, fp_); }

	// not implemented
	char* putbegin() { return 0; }
	size_t putend(char*) { return 0; }

private:
	void read() {
		rapidjson_assert(fp_ != 0);
		int c = fgetc(fp_);
		if (c != eof) {
			current_ = (char)c;
			count_++;
		}
		else
			current_ = '\0';
	}

	file* fp_;
	char current_;
	size_t count_;
};

} // namespace rapidjson

#endif // rapidjson_filestream_h_
