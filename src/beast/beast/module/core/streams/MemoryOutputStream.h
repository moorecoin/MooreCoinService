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

#ifndef beast_memoryoutputstream_h_included
#define beast_memoryoutputstream_h_included

namespace beast
{

//==============================================================================
/**
    writes data to an internal memory buffer, which grows as required.

    the data that was written into the stream can then be accessed later as
    a contiguous block of memory.
*/
//==============================================================================
/**
    writes data to an internal memory buffer, which grows as required.

    the data that was written into the stream can then be accessed later as
    a contiguous block of memory.
*/
class  memoryoutputstream
    : public outputstream
    , leakchecked <memoryoutputstream>
{
public:
    //==============================================================================
    /** creates an empty memory stream, ready to be written into.
        @param initialsize  the intial amount of capacity to allocate for writing into
    */
    memoryoutputstream (size_t initialsize = 256);

    /** creates a memory stream for writing into into a pre-existing memoryblock object.

        note that the destination block will always be larger than the amount of data
        that has been written to the stream, because the memoryoutputstream keeps some
        spare capactity at its end. to trim the block's size down to fit the actual
        data, call flush(), or delete the memoryoutputstream.

        @param memoryblocktowriteto             the block into which new data will be written.
        @param appendtoexistingblockcontent     if this is true, the contents of the block will be
                                                kept, and new data will be appended to it. if false,
                                                the block will be cleared before use
    */
    memoryoutputstream (memoryblock& memoryblocktowriteto,
                        bool appendtoexistingblockcontent);

    /** creates a memoryoutputstream that will write into a user-supplied, fixed-size
        block of memory.

        when using this mode, the stream will write directly into this memory area until
        it's full, at which point write operations will fail.
    */
    memoryoutputstream (void* destbuffer, size_t destbuffersize);

    /** destructor.
        this will free any data that was written to it.
    */
    ~memoryoutputstream();

    //==============================================================================
    /** returns a pointer to the data that has been written to the stream.
        @see getdatasize
    */
    const void* getdata() const noexcept;

    /** returns the number of bytes of data that have been written to the stream.
        @see getdata
    */
    size_t getdatasize() const noexcept                 { return size; }

    /** resets the stream, clearing any data that has been written to it so far. */
    void reset() noexcept;

    /** increases the internal storage capacity to be able to contain at least the specified
        amount of data without needing to be resized.
    */
    void preallocate (size_t bytestopreallocate);

    /** appends the utf-8 bytes for a unicode character */
    bool appendutf8char (beast_wchar character);

    /** returns a string created from the (utf8) data that has been written to the stream. */
    string toutf8() const;

    /** attempts to detect the encoding of the data and convert it to a string.
        @see string::createstringfromdata
    */
    string tostring() const;

    /** returns a copy of the stream's data as a memory block. */
    memoryblock getmemoryblock() const;

    //==============================================================================
    /** if the stream is writing to a user-supplied memoryblock, this will trim any excess
        capacity off the block, so that its length matches the amount of actual data that
        has been written so far.
    */
    void flush();

    bool write (const void*, size_t) override;
    std::int64_t getposition() override                                 { return position; }
    bool setposition (std::int64_t) override;
    int writefrominputstream (inputstream&, std::int64_t maxnumbytestowrite) override;
    bool writerepeatedbyte (std::uint8_t byte, size_t numtimestorepeat) override;

private:
    void trimexternalblocksize();
    char* preparetowrite (size_t);

    //==============================================================================
    memoryblock* const blocktouse;
    memoryblock internalblock;
    void* externaldata;
    size_t position, size, availablesize;
};

/** copies all the data that has been written to a memoryoutputstream into another stream. */
outputstream& operator<< (outputstream& stream, const memoryoutputstream& streamtoread);

} // beast

#endif
