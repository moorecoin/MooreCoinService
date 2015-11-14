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

#include <beastconfig.h>
#include <ripple/basics/log.h>
#include <boost/algorithm/string.hpp>
// vfalco todo use std::chrono
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cassert>
#include <fstream>
#include <thread>
#include <boost/format.hpp>

namespace ripple {

logs::sink::sink (std::string const& partition,
    beast::journal::severity severity, logs& logs)
    : logs_(logs)
    , partition_(partition)
{
    beast::journal::sink::severity (severity);
}

void
logs::sink::write (beast::journal::severity level, std::string const& text)
{
    logs_.write (level, partition_, text, console());
}

//------------------------------------------------------------------------------

logs::file::file()
    : m_stream (nullptr)
{
}

logs::file::~file()
{
}

bool logs::file::isopen () const noexcept
{
    return m_stream != nullptr;
}

bool logs::file::open (boost::filesystem::path const& path)
{
    close ();

    bool wasopened = false;

    // vfalco todo make this work with unicode file paths
    std::unique_ptr <std::ofstream> stream (
        new std::ofstream (path.c_str (), std::fstream::app));

    if (stream->good ())
    {
        m_path = path;

        m_stream = std::move (stream);

        wasopened = true;
    }

    return wasopened;
}

bool logs::file::closeandreopen ()
{
    close ();

    return open (m_path);
}

void logs::file::close ()
{
    m_stream = nullptr;
}

void logs::file::write (char const* text)
{
    if (m_stream != nullptr)
        (*m_stream) << text;
}

void logs::file::writeln (char const* text)
{
    if (m_stream != nullptr)
    {
        (*m_stream) << text;
        (*m_stream) << std::endl;
    }
}

//------------------------------------------------------------------------------

logs::logs()
    : level_ (beast::journal::kwarning) // default severity
{
}

bool
logs::open (boost::filesystem::path const& pathtologfile)
{
    return file_.open(pathtologfile);
}

logs::sink&
logs::get (std::string const& name)
{
    std::lock_guard <std::mutex> lock (mutex_);
    auto const result (sinks_.emplace (std::piecewise_construct,
        std::forward_as_tuple(name), std::forward_as_tuple (name,
            level_, *this)));
    return result.first->second;
}

logs::sink&
logs::operator[] (std::string const& name)
{
    return get(name);
}

beast::journal
logs::journal (std::string const& name)
{
    return beast::journal (get(name));
}

beast::journal::severity
logs::severity() const
{
    return level_;
}

void
logs::severity (beast::journal::severity level)
{
    std::lock_guard <std::mutex> lock (mutex_);
    level_ = level;
    for (auto& sink : sinks_)
        sink.second.severity (level);
}

std::vector<std::pair<std::string, std::string>>
logs::partition_severities() const
{
    std::vector<std::pair<std::string, std::string>> list;
    std::lock_guard <std::mutex> lock (mutex_);
    list.reserve (sinks_.size());
    for (auto const& e : sinks_)
        list.push_back(std::make_pair(e.first,
            tostring(fromseverity(e.second.severity()))));
    return list;
}

void
logs::write (beast::journal::severity level, std::string const& partition,
    std::string const& text, bool console)
{
    std::string s;
    format (s, text, level, partition);
    std::lock_guard <std::mutex> lock (mutex_);
    file_.writeln (s);
    std::cerr << s << '\n';
    // vfalco todo fix console output
    //if (console)
    //    out_.write_console(s);
}

std::string
logs::rotate()
{
    std::lock_guard <std::mutex> lock (mutex_);
    bool const wasopened = file_.closeandreopen ();
    if (wasopened)
        return "the log file was closed and reopened.";
    return "the log file could not be closed and reopened.";
}

logseverity
logs::fromseverity (beast::journal::severity level)
{
    using beast::journal;
    switch (level)
    {
    case journal::ktrace:   return lstrace;
    case journal::kdebug:   return lsdebug;
    case journal::kinfo:    return lsinfo;
    case journal::kwarning: return lswarning;
    case journal::kerror:   return lserror;

    default:
        assert(false);
    case journal::kfatal:
        break;
    }

    return lsfatal;
}

beast::journal::severity
logs::toseverity (logseverity level)
{
    using beast::journal;
    switch (level)
    {
    case lstrace:   return journal::ktrace;
    case lsdebug:   return journal::kdebug;
    case lsinfo:    return journal::kinfo;
    case lswarning: return journal::kwarning;
    case lserror:   return journal::kerror;
    default:
        assert(false);
    case lsfatal:
        break;
    }

    return journal::kfatal;
}

std::string
logs::tostring (logseverity s)
{
    switch (s)
    {
    case lstrace:   return "trace";
    case lsdebug:   return "debug";
    case lsinfo:    return "info";
    case lswarning: return "warning";
    case lserror:   return "error";
    case lsfatal:   return "fatal";
    default:
        assert (false);
        return "unknown";
    }
}

logseverity
logs::fromstring (std::string const& s)
{
    if (boost::iequals (s, "trace"))
        return lstrace;

    if (boost::iequals (s, "debug"))
        return lsdebug;

    if (boost::iequals (s, "info") || boost::iequals (s, "information"))
        return lsinfo;

    if (boost::iequals (s, "warn") || boost::iequals (s, "warning") || boost::iequals (s, "warnings"))
        return lswarning;

    if (boost::iequals (s, "error") || boost::iequals (s, "errors"))
        return lserror;

    if (boost::iequals (s, "fatal") || boost::iequals (s, "fatals"))
        return lsfatal;

    return lsinvalid;
}

// replace the first secret, if any, with asterisks
std::string
logs::scrub (std::string s)
{
    using namespace std;
    char const* secrettoken = "\"secret\"";
    // look for the first occurrence of "secret" in the string.
    size_t startingposition = s.find (secrettoken);
    if (startingposition != string::npos)
    {
        // found it, advance past the token.
        startingposition += strlen (secrettoken);
        // replace the next 35 characters at most, without overwriting the end.
        size_t endingposition = std::min (startingposition + 35, s.size () - 1);
        for (size_t i = startingposition; i < endingposition; ++i)
            s [i] = '*';
    }
    return s;
}

void
logs::format (std::string& output, std::string const& message,
    beast::journal::severity severity, std::string const& partition)
{
    output.reserve (message.size() + partition.size() + 100);

    output = boost::posix_time::to_simple_string (
        boost::posix_time::second_clock::universal_time ());

    output += boost::str( boost::format(" <%x>") % std::this_thread::get_id());
    
    if (! partition.empty ())
        output += partition + ":";

    switch (severity)
    {
    case beast::journal::ktrace:    output += "trc "; break;
    case beast::journal::kdebug:    output += "dbg "; break;
    case beast::journal::kinfo:     output += "nfo "; break;
    case beast::journal::kwarning:  output += "wrn "; break;
    case beast::journal::kerror:    output += "err "; break;
    default:
        assert(false);
    case beast::journal::kfatal:    output += "ftl "; break;
    }

    output += scrub (message);

    if (output.size() > maximummessagecharacters)
    {
        output.resize (maximummessagecharacters - 3);
        output += "...";
    }
}

} // ripple
