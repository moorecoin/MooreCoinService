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

#ifndef beast_inputstream_h_included
#define beast_inputstream_h_included

namespace beast
{

class memoryblock;

//==============================================================================
/** the base class for streams that read data.

    input and output streams are used throughout the library - subclasses can override
    some or all of the virtual functions to implement their behaviour.

    @see outputstream, fileinputstream
*/
class inputstream
    : leakchecked <inputstream>
{
public:
    /** destructor. */
    virtual ~inputstream()  {}

    //==============================================================================
    /** returns the total number of bytes available for reading in this stream.

        note that this is the number of bytes available from the start of the
        stream, not from the current position.

        if the size of the stream isn't actually known, this will return -1.

        @see getnumbytesremaining
    */
    virtual std::int64_t gettotallength() = 0;

    /** returns the number of bytes available for reading, or a negative value if
        the remaining length is not known.
        @see gettotallength
    */
    std::int64_t getnumbytesremaining();

    /** returns true if the stream has no more data to read. */
    virtual bool isexhausted() = 0;

    //==============================================================================
    /** reads some data from the stream into a memory buffer.

        this is the only read method that subclasses actually need to implement, as the
        inputstream base class implements the other read methods in terms of this one (although
        it's often more efficient for subclasses to implement them directly).

        @param destbuffer       the destination buffer for the data. this must not be null.
        @param maxbytestoread   the maximum number of bytes to read - make sure the
                                memory block passed in is big enough to contain this
                                many bytes. this value must not be negative.

        @returns    the actual number of bytes that were read, which may be less than
                    maxbytestoread if the stream is exhausted before it gets that far
    */
    virtual int read (void* destbuffer, int maxbytestoread) = 0;

    /** reads a byte from the stream.

        if the stream is exhausted, this will return zero.

        @see outputstream::writebyte
    */
    virtual char readbyte();

    /** reads a boolean from the stream.

        the bool is encoded as a single byte - non-zero for true, 0 for false. 

        if the stream is exhausted, this will return false.

        @see outputstream::writebool
    */
    virtual bool readbool();

    /** reads two bytes from the stream as a little-endian 16-bit value.

        if the next two bytes read are byte1 and byte2, this returns
        (byte1 | (byte2 << 8)).

        if the stream is exhausted partway through reading the bytes, this will return zero.

        @see outputstream::writeshort, readshortbigendian
    */
    virtual short readshort();

    // vfalco todo implement these functions
    //virtual std::int16_t readint16 ();
    //virtual std::uint16_t readuint16 ();

    /** reads two bytes from the stream as a little-endian 16-bit value.

        if the next two bytes read are byte1 and byte2, this returns (byte1 | (byte2 << 8)). 

        if the stream is exhausted partway through reading the bytes, this will return zero.

        @see outputstream::writeshortbigendian, readshort
    */
    virtual short readshortbigendian();

    /** reads four bytes from the stream as a little-endian 32-bit value.

        if the next four bytes are byte1 to byte4, this returns
        (byte1 | (byte2 << 8) | (byte3 << 16) | (byte4 << 24)).

        if the stream is exhausted partway through reading the bytes, this will return zero.

        @see outputstream::writeint, readintbigendian
    */
    virtual std::int32_t readint32();

    // vfalco todo implement these functions
    //virtual std::int16_t readint16bigendian ();
    //virtual std::uint16_t readuint16bigendian ();

    // deprecated, assumes sizeof(int) == 4!
    virtual int readint();

    /** reads four bytes from the stream as a big-endian 32-bit value.

        if the next four bytes are byte1 to byte4, this returns
        (byte4 | (byte3 << 8) | (byte2 << 16) | (byte1 << 24)).

        if the stream is exhausted partway through reading the bytes, this will return zero.

        @see outputstream::writeintbigendian, readint
    */
    virtual std::int32_t readint32bigendian();

    // deprecated, assumes sizeof(int) == 4!
    virtual int readintbigendian();

    /** reads eight bytes from the stream as a little-endian 64-bit value.

        if the next eight bytes are byte1 to byte8, this returns
        (byte1 | (byte2 << 8) | (byte3 << 16) | (byte4 << 24) | (byte5 << 32) | (byte6 << 40) | (byte7 << 48) | (byte8 << 56)).

        if the stream is exhausted partway through reading the bytes, this will return zero.

        @see outputstream::writeint64, readint64bigendian
    */
    virtual std::int64_t readint64();

    /** reads eight bytes from the stream as a big-endian 64-bit value.

        if the next eight bytes are byte1 to byte8, this returns
        (byte8 | (byte7 << 8) | (byte6 << 16) | (byte5 << 24) | (byte4 << 32) | (byte3 << 40) | (byte2 << 48) | (byte1 << 56)).

        if the stream is exhausted partway through reading the bytes, this will return zero.

        @see outputstream::writeint64bigendian, readint64
    */
    virtual std::int64_t readint64bigendian();

    /** reads four bytes as a 32-bit floating point value.

        the raw 32-bit encoding of the float is read from the stream as a little-endian int.

        if the stream is exhausted partway through reading the bytes, this will return zero.

        @see outputstream::writefloat, readdouble
    */
    virtual float readfloat();

    /** reads four bytes as a 32-bit floating point value.

        the raw 32-bit encoding of the float is read from the stream as a big-endian int.

        if the stream is exhausted partway through reading the bytes, this will return zero.

        @see outputstream::writefloatbigendian, readdoublebigendian
    */
    virtual float readfloatbigendian();

    /** reads eight bytes as a 64-bit floating point value.

        the raw 64-bit encoding of the double is read from the stream as a little-endian std::int64_t.

        if the stream is exhausted partway through reading the bytes, this will return zero.

        @see outputstream::writedouble, readfloat
    */
    virtual double readdouble();

    /** reads eight bytes as a 64-bit floating point value.

        the raw 64-bit encoding of the double is read from the stream as a big-endian std::int64_t.

        if the stream is exhausted partway through reading the bytes, this will return zero.

        @see outputstream::writedoublebigendian, readfloatbigendian
    */
    virtual double readdoublebigendian();

    /** reads an encoded 32-bit number from the stream using a space-saving compressed format.

        for small values, this is more space-efficient than using readint() and outputstream::writeint()

        the format used is: number of significant bytes + up to 4 bytes in little-endian order.

        @see outputstream::writecompressedint()
    */
    virtual int readcompressedint();

    /** reads a type using a template specialization.

        this is useful when doing template meta-programming.
    */
    template <class t>
    t readtype ();

    /** reads a type using a template specialization.

        the variable is passed as a parameter so that the template type
        can be deduced. the return value indicates whether or not there
        was sufficient data in the stream to read the value.

    */
    template <class t>
    bool readtypeinto (t* p)
    {
        if (getnumbytesremaining () >= sizeof (t))
        {
            *p = readtype <t> ();
            return true;
        }
        return false;
    }

    /** reads a type from a big endian stream using a template specialization.

        the raw encoding of the type is read from the stream as a big-endian value
        where applicable.

        this is useful when doing template meta-programming.
    */
    template <class t>
    t readtypebigendian ();

    /** reads a type using a template specialization.

        the variable is passed as a parameter so that the template type
        can be deduced. the return value indicates whether or not there
        was sufficient data in the stream to read the value.
    */
    template <class t>
    bool readtypebigendianinto (t* p)
    {
        if (getnumbytesremaining () >= sizeof (t))
        {
            *p = readtypebigendian <t> ();
            return true;
        }
        return false;
    }

    //==============================================================================
    /** reads a utf-8 string from the stream, up to the next linefeed or carriage return.

        this will read up to the next "\n" or "\r\n" or end-of-stream.

        after this call, the stream's position will be left pointing to the next character
        following the line-feed, but the linefeeds aren't included in the string that
        is returned.
    */
    virtual string readnextline();

    /** reads a zero-terminated utf-8 string from the stream.

        this will read characters from the stream until it hits a null character
        or end-of-stream.

        @see outputstream::writestring, readentirestreamasstring
    */
    virtual string readstring();

    /** tries to read the whole stream and turn it into a string.

        this will read from the stream's current position until the end-of-stream.
        it can read from either utf-16 or utf-8 formats.
    */
    virtual string readentirestreamasstring();

    /** reads from the stream and appends the data to a memoryblock.

        @param destblock            the block to append the data onto
        @param maxnumbytestoread    if this is a positive value, it sets a limit to the number
                                    of bytes that will be read - if it's negative, data
                                    will be read until the stream is exhausted.
        @returns the number of bytes that were added to the memory block
    */
    virtual int readintomemoryblock (memoryblock& destblock,
                                     std::ptrdiff_t maxnumbytestoread = -1);

    //==============================================================================
    /** returns the offset of the next byte that will be read from the stream.

        @see setposition
    */
    virtual std::int64_t getposition() = 0;

    /** tries to move the current read position of the stream.

        the position is an absolute number of bytes from the stream's start.

        some streams might not be able to do this, in which case they should do
        nothing and return false. others might be able to manage it by resetting
        themselves and skipping to the correct position, although this is
        obviously a bit slow.

        @returns  true if the stream manages to reposition itself correctly
        @see getposition
    */
    virtual bool setposition (std::int64_t newposition) = 0;

    /** reads and discards a number of bytes from the stream.

        some input streams might implement this efficiently, but the base
        class will just keep reading data until the requisite number of bytes
        have been done.
    */
    virtual void skipnextbytes (std::int64_t numbytestoskip);


protected:
    //==============================================================================
    inputstream() = default;
    inputstream (inputstream const&) = delete;
    inputstream& operator= (inputstream const&) = delete;
};

} // beast

#endif
