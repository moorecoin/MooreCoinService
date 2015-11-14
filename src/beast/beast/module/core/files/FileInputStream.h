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

#ifndef beast_fileinputstream_h_included
#define beast_fileinputstream_h_included

namespace beast
{

//==============================================================================
/**
    an input stream that reads from a local file.

    @see inputstream, fileoutputstream, file::createinputstream
*/
class fileinputstream
    : public inputstream
    , leakchecked <fileinputstream>
{
public:
    //==============================================================================
    /** creates a fileinputstream.

        @param filetoread   the file to read from - if the file can't be accessed for some
                            reason, then the stream will just contain no data
    */
    explicit fileinputstream (const file& filetoread);

    /** destructor. */
    ~fileinputstream();

    //==============================================================================
    /** returns the file that this stream is reading from. */
    const file& getfile() const noexcept                { return file; }

    /** returns the status of the file stream.
        the result will be ok if the file opened successfully. if an error occurs while
        opening or reading from the file, this will contain an error message.
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


    //==============================================================================
    std::int64_t gettotallength();
    int read (void* destbuffer, int maxbytestoread);
    bool isexhausted();
    std::int64_t getposition();
    bool setposition (std::int64_t pos);

private:
    //==============================================================================
    file file;
    void* filehandle;
    std::int64_t currentposition;
    result status;
    bool needtoseek;

    void openhandle();
    void closehandle();
    size_t readinternal (void* buffer, size_t numbytes);
};

} // beast

#endif   // beast_fileinputstream_h_included
