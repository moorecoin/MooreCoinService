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

#ifndef beast_utility_std_ostream_h_included
#define beast_utility_std_ostream_h_included

#include <beast/streams/basic_abstract_ostream.h>

#include <ostream>

namespace beast {

/** wraps an existing std::basic_ostream as an abstract_ostream. */
template <
    class chart,
    class traits = std::char_traits <chart>
>
class basic_std_ostream
    : public basic_abstract_ostream <chart, traits>
{
private:
    using typename basic_abstract_ostream <chart, traits>::string_type;

    std::reference_wrapper <std::ostream> m_stream;

public:
    explicit basic_std_ostream (
        std::basic_ostream <chart, traits>& stream)
        : m_stream (stream)
    {
    }

    void
    write (string_type const& s) override
    {
        m_stream.get() << s << std::endl;
    }
};

typedef basic_std_ostream <char> std_ostream;

//------------------------------------------------------------------------------

/** returns a basic_std_ostream using template argument deduction. */
template <
    class chart,
    class traits = std::char_traits <chart>
>
basic_std_ostream <chart, traits>
make_std_ostream (std::basic_ostream <chart, traits>& stream)
{
    return basic_std_ostream <chart, traits> (stream);
}

}

#endif
