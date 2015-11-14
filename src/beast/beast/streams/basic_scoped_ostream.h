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

#ifndef beast_streams_basic_scoped_ostream_h_included
#define beast_streams_basic_scoped_ostream_h_included

#include <beast/cxx14/memory.h> // <memory>

#include <functional>
#include <memory>
#include <sstream>

// gcc libstd++ doesn't have move constructors for basic_ostringstream
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54316
//
#ifndef beast_no_stdlib_stream_move
# ifdef __glibcxx__
#  define beast_no_stdlib_stream_move 1
# else
#  define beast_no_stdlib_stream_move 0
# endif
#endif

namespace beast {

template <
    class chart,
    class traits
>
class basic_abstract_ostream;

/** scoped output stream that forwards to a functor upon destruction. */
template <
    class chart,
    class traits = std::char_traits <chart>,
    class allocator = std::allocator <chart>
>
class basic_scoped_ostream
{
private:
    typedef std::function <void (
        std::basic_string <chart, traits, allocator> const&)> handler_t;

    typedef std::basic_ostringstream <
        chart, traits, allocator> stream_type;

    handler_t m_handler;

#if beast_no_stdlib_stream_move
    std::unique_ptr <stream_type> m_ss;

    stream_type& stream()
    {
        return *m_ss;
    }

#else
    stream_type m_ss;

    stream_type& stream()
    {
        return m_ss;
    }

#endif

public:
    typedef std::basic_string <chart, traits> string_type;

    // disallow copy since that would duplicate the output
    basic_scoped_ostream (basic_scoped_ostream const&) = delete;
    basic_scoped_ostream& operator= (basic_scoped_ostream const) = delete;

    template <class handler>
    explicit basic_scoped_ostream (handler&& handler)
        : m_handler (std::forward <handler> (handler))
    #if beast_no_stdlib_stream_move
        , m_ss (std::make_unique <stream_type>())
    #endif
    {
    }

    template <class t, class handler>
    basic_scoped_ostream (t const& t, handler&& handler)
        : m_handler (std::forward <handler> (handler))
    #if beast_no_stdlib_stream_move
        , m_ss (std::make_unique <stream_type>())
    #endif
    {
        stream() << t;
    }

    basic_scoped_ostream (basic_abstract_ostream <
        chart, traits>& ostream)
        : m_handler (
            [&](string_type const& s)
            {
                ostream.write (s);
            })
    {
    }

    basic_scoped_ostream (basic_scoped_ostream&& other)
        : m_handler (std::move (other.m_handler))
        , m_ss (std::move (other.m_ss))
    {
    }

    ~basic_scoped_ostream()
    {
        auto const& s (stream().str());
        if (! s.empty())
            m_handler (s);
    }

    basic_scoped_ostream&
    operator<< (std::ostream& manip (std::ostream&))
    {
        stream() << manip;
        return *this;
    }

    template <class t>
    basic_scoped_ostream&
    operator<< (t const& t)
    {
        stream() << t;
        return *this;
    }
};

}

#endif
