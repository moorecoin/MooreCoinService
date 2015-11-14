//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_unittestutilities_h_included
#define beast_unittestutilities_h_included

#include <beast/module/core/files/file.h>
#include <beast/module/core/maths/random.h>

namespace beast {
namespace unittestutilities {

/** fairly shuffle an array pseudo-randomly.
*/
template <class t>
void repeatableshuffle (int const numberofitems, t& arrayofitems, random& r)
{
    for (int i = numberofitems - 1; i > 0; --i)
    {
        int const choice = r.nextint (i + 1);
        std::swap (arrayofitems [i], arrayofitems [choice]);
    }
}

template <class t>
void repeatableshuffle (int const numberofitems, t& arrayofitems, std::int64_t seedvalue)
{
    random r (seedvalue);
    repeatableshuffle (numberofitems, arrayofitems, r);
}

//------------------------------------------------------------------------------

/** a block of memory used for test data.
*/
struct payload
{
    /** construct a payload with a buffer of the specified maximum size.

        @param maximumbytes the size of the buffer, in bytes.
    */
    explicit payload (int maxbuffersize)
        : buffersize (maxbuffersize)
        , data (maxbuffersize)
    {
    }

    /** generate a random block of data within a certain size range.

        @param minimumbytes the smallest number of bytes in the resulting payload.
        @param maximumbytes the largest number of bytes in the resulting payload.
        @param seedvalue the value to seed the random number generator with.
    */
    void repeatablerandomfill (int minimumbytes, int maximumbytes, std::int64_t seedvalue) noexcept
    {
        bassert (minimumbytes >=0 && maximumbytes <= buffersize);

        random r (seedvalue);

        bytes = minimumbytes + r.nextint (1 + maximumbytes - minimumbytes);

        bassert (bytes >= minimumbytes && bytes <= buffersize);

        for (int i = 0; i < bytes; ++i)
            data [i] = static_cast <unsigned char> (r.nextint ());
    }

    /** compare two payloads for equality.
    */
    bool operator== (payload const& other) const noexcept
    {
        if (bytes == other.bytes)
        {
            return memcmp (data.getdata (), other.data.getdata (), bytes) == 0;
        }
        else
        {
            return false;
        }
    }

public:
    int const buffersize;

    int bytes;
    heapblock <char> data;
};

class tempdirectory
{
public:
    explicit tempdirectory (std::string const& root)
            : directory (file::createtempfile (root))
    {
    }

    ~tempdirectory()
    {
        directory.deleterecursively();
    }

    string const& getfullpathname() const
    {
        return directory.getfullpathname();
    }

    tempdirectory(const tempdirectory&) = delete;
    tempdirectory& operator=(const tempdirectory&) = delete;

private:
    file const directory;
};

} // unittestutilities
} // beast

#endif
