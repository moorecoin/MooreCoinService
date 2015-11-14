//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

    permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    the  software is provided "as is" and the author disclaims all warranties
    with  regard  to  this  software  including  all  implied  warranties  of
    merchantability  and  fitness. in no event shall the author be liable for
    any  special ,  direct, indirect, or consequential damages or any damages
    whatsoever  resulting  from  loss  of use, data or profits, whether in an
    action  of  contract, negligence or other tortious action, arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

#ifndef beast_outputstream_h_included
#define beast_outputstream_h_included

namespace beast
{

class inputstream;
class memoryblock;
class file;

//==============================================================================
/**
    the base class for streams that write data to some kind of destination.

    input and output streams are used throughout the library - subclasses can override
    some or all of the virtual functions to implement their behaviour.

    @see inputstream, memoryoutputstream, fileoutputstream
*/
class  outputstream
{
protected:
    //==============================================================================
    outputstream();

    outputstream (outputstream const&) = delete;
    outputstream& operator= (outputstream const&) = delete;

public:
    /** destructor.

        some subclasses might want to do things like call flush() during their
        destructors.
    */
    virtual ~outputstream();

    //==============================================================================
    /** if the stream is using a buffer, this will ensure it gets written
        out to the destination. */
    virtual void flush() = 0;

    /** tries to move the stream's output position.

        not all streams will be able to seek to a new position - this will return
        false if it fails to work.

        @see getposition
    */
    virtual bool setposition (std::int64_t newposition) = 0;

    /** returns the stream's current position.

        @see setposition
    */
    virtual std::int64_t getposition() = 0;

    //==============================================================================
    /** writes a block of data to the stream.

        when creating a subclass of outputstream, this is the only write method
        that needs to be overloaded - the base class has methods for writing other
        types of data which use this to do the work.

        @param datatowrite      the target buffer to receive the data. this must not be null.
        @param numberofbytes    the number of bytes to write.
        @returns false if the write operation fails for some reason
    */
    virtual bool write (const void* datatowrite,
                        size_t numberofbytes) = 0;

    //==============================================================================
    /** writes a single byte to the stream.
        @returns false if the write operation fails for some reason
        @see inputstream::readbyte
    */
    virtual bool writebyte (char byte);

    /** writes a boolean to the stream as a single byte.
        this is encoded as a binary byte (not as text) with a value of 1 or 0.
        @returns false if the write operation fails for some reason
        @see inputstream::readbool
    */
    virtual bool writebool (bool boolvalue);

    /** writes a 16-bit integer to the stream in a little-endian byte order.
        this will write two bytes to the stream: (value & 0xff), then (value >> 8).
        @returns false if the write operation fails for some reason
        @see inputstream::readshort
    */
    virtual bool writeshort (short value);

    /** writes a 16-bit integer to the stream in a big-endian byte order.
        this will write two bytes to the stream: (value >> 8), then (value & 0xff).
        @returns false if the write operation fails for some reason
        @see inputstream::readshortbigendian
    */
    virtual bool writeshortbigendian (short value);

    /** writes a 32-bit integer to the stream in a little-endian byte order.
        @returns false if the write operation fails for some reason
        @see inputstream::readint
    */
    virtual bool writeint32 (std::int32_t value);

    // deprecated, assumes sizeof (int) == 4!
    virtual bool writeint (int value);

    /** writes a 32-bit integer to the stream in a big-endian byte order.
        @returns false if the write operation fails for some reason
        @see inputstream::readintbigendian
    */
    virtual bool writeint32bigendian (std::int32_t value);

    // deprecated, assumes sizeof (int) == 4!
    virtual bool writeintbigendian (int value);

    /** writes a 64-bit integer to the stream in a little-endian byte order.
        @returns false if the write operation fails for some reason
        @see inputstream::readint64
    */
    virtual bool writeint64 (std::int64_t value);

    /** writes a 64-bit integer to the stream in a big-endian byte order.
        @returns false if the write operation fails for some reason
        @see inputstream::readint64bigendian
    */
    virtual bool writeint64bigendian (std::int64_t value);

    /** writes a 32-bit floating point value to the stream in a binary format.
        the binary 32-bit encoding of the float is written as a little-endian int.
        @returns false if the write operation fails for some reason
        @see inputstream::readfloat
    */
    virtual bool writefloat (float value);

    /** writes a 32-bit floating point value to the stream in a binary format.
        the binary 32-bit encoding of the float is written as a big-endian int.
        @returns false if the write operation fails for some reason
        @see inputstream::readfloatbigendian
    */
    virtual bool writefloatbigendian (float value);

    /** writes a 64-bit floating point value to the stream in a binary format.
        the eight raw bytes of the double value are written out as a little-endian 64-bit int.
        @returns false if the write operation fails for some reason
        @see inputstream::readdouble
    */
    virtual bool writedouble (double value);

    /** writes a 64-bit floating point value to the stream in a binary format.
        the eight raw bytes of the double value are written out as a big-endian 64-bit int.
        @see inputstream::readdoublebigendian
        @returns false if the write operation fails for some reason
    */
    virtual bool writedoublebigendian (double value);

    /** write a type using a template specialization.

        this is useful when doing template meta-programming.
    */
    template <class t>
    bool writetype (t value);

    /** write a type using a template specialization.

        the raw encoding of the type is written to the stream as a big-endian value
        where applicable.

        this is useful when doing template meta-programming.
    */
    template <class t>
    bool writetypebigendian (t value);

    /** writes a byte to the output stream a given number of times.
        @returns false if the write operation fails for some reason
    */
    virtual bool writerepeatedbyte (std::uint8_t byte, size_t numtimestorepeat);

    /** writes a condensed binary encoding of a 32-bit integer.

        if you're storing a lot of integers which are unlikely to have very large values,
        this can save a lot of space, because values under 0xff will only take up 2 bytes,
        under 0xffff only 3 bytes, etc.

        the format used is: number of significant bytes + up to 4 bytes in little-endian order.

        @returns false if the write operation fails for some reason
        @see inputstream::readcompressedint
    */
    virtual bool writecompressedint (int value);

    /** stores a string in the stream in a binary format.

        this isn't the method to use if you're trying to append text to the end of a
        text-file! it's intended for storing a string so that it can be retrieved later
        by inputstream::readstring().

        it writes the string to the stream as utf8, including the null termination character.

        for appending text to a file, instead use writetext, or operator<<

        @returns false if the write operation fails for some reason
        @see inputstream::readstring, writetext, operator<<
    */
    virtual bool writestring (const string& text);

    /** writes a string of text to the stream.

        it can either write the text as utf-8 or utf-16, and can also add the utf-16 byte-order-mark
        bytes (0xff, 0xfe) to indicate the endianness (these should only be used at the start
        of a file).

        the method also replaces '\\n' characters in the text with '\\r\\n'.
        @returns false if the write operation fails for some reason
    */
    virtual bool writetext (const string& text,
                            bool asutf16,
                            bool writeutf16byteordermark);

    /** reads data from an input stream and writes it to this stream.

        @param source               the stream to read from
        @param maxnumbytestowrite   the number of bytes to read from the stream (if this is
                                    less than zero, it will keep reading until the input
                                    is exhausted)
        @returns the number of bytes written
    */
    virtual int writefrominputstream (inputstream& source, std::int64_t maxnumbytestowrite);

    //==============================================================================
    /** sets the string that will be written to the stream when the writenewline()
        method is called.
        by default this will be set the the value of newline::getdefault().
    */
    void setnewlinestring (const string& newlinestring);

    /** returns the current new-line string that was set by setnewlinestring(). */
    const string& getnewlinestring() const noexcept         { return newlinestring; }

private:
    //==============================================================================
    string newlinestring;
};

//==============================================================================
/** writes a number to a stream as 8-bit characters in the default system encoding. */
outputstream& operator<< (outputstream& stream, int number);

/** writes a number to a stream as 8-bit characters in the default system encoding. */
outputstream& operator<< (outputstream& stream, std::int64_t number);

/** writes a number to a stream as 8-bit characters in the default system encoding. */
outputstream& operator<< (outputstream& stream, double number);

/** writes a character to a stream. */
outputstream& operator<< (outputstream& stream, char character);

/** writes a null-terminated text string to a stream. */
outputstream& operator<< (outputstream& stream, const char* text);

/** writes a block of data from a memoryblock to a stream. */
outputstream& operator<< (outputstream& stream, const memoryblock& data);

/** writes the contents of a file to a stream. */
outputstream& operator<< (outputstream& stream, const file& filetoread);

/** writes the complete contents of an input stream to an output stream. */
outputstream& operator<< (outputstream& stream, inputstream& streamtoread);

/** writes a new-line to a stream.
    you can use the predefined symbol 'newline' to invoke this, e.g.
    @code
    myoutputstream << "hello world" << newline << newline;
    @endcode
    @see outputstream::setnewlinestring
*/
outputstream& operator<< (outputstream& stream, const newline&);

/** writes a string to an outputstream as utf8. */
outputstream& operator<< (outputstream& stream, const string& stringtowrite);

} // beast

#endif
