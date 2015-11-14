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

#ifndef beast_streams_basic_abstract_ostream_h_included
#define beast_streams_basic_abstract_ostream_h_included

#include <beast/streams/basic_scoped_ostream.h>

#include <functional>
#include <memory>
#include <sstream>

namespace beast {

/** abstraction for an output stream similar to std::basic_ostream. */
template <
    class chart,
    class traits = std::char_traits <chart>
>
class basic_abstract_ostream
{
public:
    typedef std::basic_string <chart, traits> string_type;
    typedef basic_scoped_ostream <chart, traits> scoped_stream_type;

    basic_abstract_ostream() = default;

    virtual
    ~basic_abstract_ostream() = default;

    basic_abstract_ostream (basic_abstract_ostream const&) = default;
    basic_abstract_ostream& operator= (
        basic_abstract_ostream const&) = default;

    /** returns `true` if the stream is active.
        inactive streams do not produce output.
    */
    /** @{ */
    virtual
    bool
    active() const
    {
        return true;
    }

    explicit
    operator bool() const
    {
        return active();
    }
    /** @} */

    /** called to output each string. */
    virtual
    void
    write (string_type const& s) = 0;

    scoped_stream_type
    operator<< (std::ostream& manip (std::ostream&))
    {
        return scoped_stream_type (manip,
            [this](string_type const& s)
            {
                this->write (s);
            });
    }

    template <class t>
    scoped_stream_type
    operator<< (t const& t)
    {
        return scoped_stream_type (t,
            [this](string_type const& s)
            {
                this->write (s);
            });
    }
};

}

#endif
