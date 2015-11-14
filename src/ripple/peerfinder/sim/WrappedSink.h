//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#ifndef ripple_peerfinder_sim_wrappedsink_h_included
#define ripple_peerfinder_sim_wrappedsink_h_included

namespace ripple {
namespace peerfinder {

/** wraps a journal::sink to prefix it's output. */
class wrappedsink : public beast::journal::sink
{
public:
    wrappedsink (std::string const& prefix, beast::journal::sink& sink)
        : m_prefix (prefix)
        , m_sink (sink)
    {
    }

    bool active (beast::journal::severity level) const
        { return m_sink.active (level); }

    bool console () const
        { return m_sink.console (); }

    void console (bool output)
        { m_sink.console (output); }

    beast::journal::severity severity() const
        { return m_sink.severity(); }

    void severity (beast::journal::severity level)
        { m_sink.severity (level); }

    void write (beast::journal::severity level, std::string const& text)
    {
        using beast::journal;
        std::string s (m_prefix);
        switch (level)
        {
        case journal::ktrace:   s += "trace: "; break;
        case journal::kdebug:   s += "debug: "; break;
        case journal::kinfo:    s += "info : "; break;
        case journal::kwarning: s += "warn : "; break;
        case journal::kerror:   s += "error: "; break;
        default:
        case journal::kfatal:   s += "fatal: "; break;
        };

        s+= text;
        m_sink.write (level, s);
    }

private:
    std::string const m_prefix;
    beast::journal::sink& m_sink;
};

}
}

#endif
