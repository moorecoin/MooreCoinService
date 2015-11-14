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

namespace beast
{

#ifndef beast_fileoutputstream_h_included
#define beast_fileoutputstream_h_included

//==============================================================================
/**
    an output stream that writes into a local file.

    @see outputstream, fileinputstream, file::createoutputstream
*/
class fileoutputstream
    : public outputstream
    , leakchecked <fileoutputstream>
{
public:
    //==============================================================================
    /** creates a fileoutputstream.

        if the file doesn't exist, it will first be created. if the file can't be
        created or opened, the failedtoopen() method will return
        true.

        if the file already exists when opened, the stream's write-postion will
        be set to the end of the file. to overwrite an existing file,
        use file::deletefile() before opening the stream, or use setposition(0)
        after it's opened (although this won't truncate the file).
    */
    fileoutputstream (const file& filetowriteto,
                      size_t buffersizetouse = 16384);

    /** destructor. */
    ~fileoutputstream();

    //==============================================================================
    /** returns the file that this stream is writing to.
    */
    const file& getfile() const                         { return file; }

    /** returns the status of the file stream.
        the result will be ok if the file opened successfully. if an error occurs while
        opening or writing to the file, this will contain an error message.
    */
    const result& getstatus() const noexcept            { return status; }

    /** returns true if the stream couldn't be opened for some reason.
        @see getresult()
    */
    bool failedtoopen() const noexcept                  { return status.failed(); }

    /** returns true if the stream opened without problems.
        @see getresult()
    */
    bool openedok() const noexcept                      { return status.wasok(); }

    /** attempts to truncate the file to the current write position.
        to truncate a file to a specific size, first use setposition() to seek to the
        appropriate location, and then call this method.
    */
    result truncate();

    //==============================================================================
    void flush() override;
    std::int64_t getposition() override;
    bool setposition (std::int64_t) override;
    bool write (const void*, size_t) override;
    bool writerepeatedbyte (std::uint8_t byte, size_t numtimestorepeat) override;


private:
    //==============================================================================
    file file;
    void* filehandle;
    result status;
    std::int64_t currentposition;
    size_t buffersize, bytesinbuffer;
    heapblock <char> buffer;

    void openhandle();
    void closehandle();
    void flushinternal();
    bool flushbuffer();
    std::int64_t setpositioninternal (std::int64_t);
    std::ptrdiff_t writeinternal (const void*, size_t);
};

} // beast

#endif