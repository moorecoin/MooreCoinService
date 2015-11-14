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

#ifndef beast_utility_wrapped_sink_h_included
#define beast_utility_wrapped_sink_h_included

#include <beast/utility/journal.h>

namespace beast {

/** wraps a journal::sink to prefix its output with a string. */
class wrappedsink : public beast::journal::sink
{
private:
    beast::journal::sink& sink_;
    std::string prefix_;

public:
    explicit
    wrappedsink (beast::journal::sink& sink, std::string const& prefix = "")
        : sink_(sink)
        , prefix_(prefix)
    {
    }

    explicit
    wrappedsink (beast::journal const& journal, std::string const& prefix = "")
        : sink_(journal.sink())
        , prefix_(prefix)
    {
    }

    void prefix (std::string const& s)
    {
        prefix_ = s;
    }

    bool
    active (beast::journal::severity level) const override
    {
        return sink_.active (level);
    }

    bool
    console () const override
    {
        return sink_.console ();
    }

    void console (bool output) override
    {
        sink_.console (output);
    }

    beast::journal::severity
    severity() const
    {
        return sink_.severity();
    }

    void severity (beast::journal::severity level)
    {
        sink_.severity (level);
    }

    void write (beast::journal::severity level, std::string const& text)
    {
        using beast::journal;
        sink_.write (level, prefix_ + text);
    }
};

}

#endif
