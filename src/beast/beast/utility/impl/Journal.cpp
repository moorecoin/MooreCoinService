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

#include <beast/utility/journal.h>
#include <beast/module/core/memory/sharedsingleton.h>

namespace beast {

//------------------------------------------------------------------------------

// a sink that does nothing.
class nulljournalsink : public journal::sink
{
public:
    bool active (journal::severity) const
    {
        return false;
    }

    bool console() const
    {
        return false;
    }

    void console (bool)
    {
    }

    journal::severity severity() const
    {
        return journal::kdisabled;
    }

    void severity (journal::severity)
    {
    }

    void write (journal::severity, std::string const&)
    {
    }
};

//------------------------------------------------------------------------------

journal::sink& journal::getnullsink ()
{
    return *sharedsingleton <nulljournalsink>::get (
        singletonlifetime::neverdestroyed);
}

//------------------------------------------------------------------------------

journal::sink::sink ()
    : m_level (kwarning)
    , m_console (false)
{
}

journal::sink::~sink ()
{
}

bool journal::sink::active (severity level) const
{
    return level >= m_level;
}

bool journal::sink::console () const
{
    return m_console;
}

void journal::sink::console (bool output)
{
    m_console = output;
}

journal::severity journal::sink::severity () const
{
    return m_level;
}

void journal::sink::severity (severity level)
{
    m_level = level;
}

//------------------------------------------------------------------------------

journal::scopedstream::scopedstream (stream const& stream)
    : m_sink (stream.sink ())
    , m_level (stream.severity ())
    , m_active (stream.active ())
{
    init ();
}

journal::scopedstream::scopedstream (scopedstream const& other)
    : m_sink (other.m_sink)
    , m_level (other.m_level)
    , m_active (other.m_active)
{
    init ();
}

journal::scopedstream::scopedstream (
    stream const& stream, std::ostream& manip (std::ostream&))
    : m_sink (stream.sink ())
    , m_level (stream.severity ())
    , m_active (stream.active ())
{
    init ();
    if (active ())
        m_ostream << manip;
}

journal::scopedstream::~scopedstream ()
{
    if (active ())
    {
        std::string const& s (m_ostream.str());
        if (! s.empty ())
        {
            if (s == "\n")
                m_sink.write (m_level, "");
            else
                m_sink.write (m_level, s);
        }
    }
}

void journal::scopedstream::init ()
{
    // modifiers applied from all ctors
    m_ostream
        << std::boolalpha
        << std::showbase
        //<< std::hex
        ;

}

std::ostream& journal::scopedstream::operator<< (std::ostream& manip (std::ostream&)) const
{
    return m_ostream << manip;
}

std::ostringstream& journal::scopedstream::ostream () const
{
    return m_ostream;
}

//------------------------------------------------------------------------------

journal::stream::stream ()
    : m_sink (&getnullsink ())
    , m_level (kdisabled)
    , m_disabled (true)
{
}

journal::stream::stream (sink& sink, severity level, bool active)
    : m_sink (&sink)
    , m_level (level)
    , m_disabled (! active)
{
    bassert (level != kdisabled);
}

journal::stream::stream (stream const& stream, bool active)
    : m_sink (&stream.sink ())
    , m_level (stream.severity ())
    , m_disabled (! active)
{
}

journal::stream::stream (stream const& other)
    : m_sink (other.m_sink)
    , m_level (other.m_level)
    , m_disabled (other.m_disabled)
{
}

journal::sink& journal::stream::sink () const
{
    return *m_sink;
}

journal::severity journal::stream::severity () const
{
    return m_level;
}

bool journal::stream::active () const
{
    return ! m_disabled && m_sink->active (m_level);
}

journal::stream& journal::stream::operator= (stream const& other)
{
    m_sink = other.m_sink;
    m_level = other.m_level;
    return *this;
}

journal::scopedstream journal::stream::operator<< (
    std::ostream& manip (std::ostream&)) const
{
    return scopedstream (*this, manip);
}

//------------------------------------------------------------------------------

journal::journal ()
    : m_sink  (&getnullsink())
    , m_level (kdisabled)
    , trace   (stream (ktrace))
    , debug   (stream (kdebug))
    , info    (stream (kinfo))
    , warning (stream (kwarning))
    , error   (stream (kerror))
    , fatal   (stream (kfatal))
{
}

journal::journal (sink& sink, severity level)
    : m_sink  (&sink)
    , m_level (level)
    , trace   (stream (ktrace))
    , debug   (stream (kdebug))
    , info    (stream (kinfo))
    , warning (stream (kwarning))
    , error   (stream (kerror))
    , fatal   (stream (kfatal))
{
}

journal::journal (journal const& other)
    : m_sink  (other.m_sink)
    , m_level (other.m_level)
    , trace   (stream (ktrace))
    , debug   (stream (kdebug))
    , info    (stream (kinfo))
    , warning (stream (kwarning))
    , error   (stream (kerror))
    , fatal   (stream (kfatal))
{
}

journal::journal (journal const& other, severity level)
    : m_sink  (other.m_sink)
    , m_level (std::max (other.m_level, level))
    , trace   (stream (ktrace))
    , debug   (stream (kdebug))
    , info    (stream (kinfo))
    , warning (stream (kwarning))
    , error   (stream (kerror))
    , fatal   (stream (kfatal))
{
}

journal::~journal ()
{
}

journal::sink& journal::sink() const
{
    return *m_sink;
}

journal::stream journal::stream (severity level) const
{
    return stream (*m_sink, level, level >= m_level);
}

bool journal::active (severity level) const
{
    if (level == kdisabled)
        return false;
    if (level < m_level)
        return false;
    return m_sink->active (level);
}

journal::severity journal::severity () const
{
    return m_level;
}

}
