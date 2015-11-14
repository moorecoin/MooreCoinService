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

#ifndef ripple_basics_log_h_included
#define ripple_basics_log_h_included

#include <ripple/basics/unorderedcontainers.h>
#include <beast/utility/ci_char_traits.h>
#include <beast/utility/journal.h>
#include <beast/utility/noexcept.h>
#include <boost/filesystem.hpp>
#include <map>
#include <mutex>
#include <utility>

namespace ripple {

// deprecated use beast::journal::severity instead
enum logseverity
{
    lsinvalid   = -1,   // used to indicate an invalid severity
    lstrace     = 0,    // very low-level progress information, details inside an operation
    lsdebug     = 1,    // function-level progress information, operations
    lsinfo      = 2,    // server-level progress information, major operations
    lswarning   = 3,    // conditions that warrant human attention, may indicate a problem
    lserror     = 4,    // a condition that indicates a problem
    lsfatal     = 5     // a severe condition that indicates a server problem
};

/** manages partitions for logging. */
class logs
{
private:
    class sink : public beast::journal::sink
    {
    private:
        logs& logs_;
        std::string partition_;

    public:
        sink (std::string const& partition,
            beast::journal::severity severity, logs& logs);

        sink (sink const&) = delete;
        sink& operator= (sink const&) = delete;

        void
        write (beast::journal::severity level, std::string const& text) override;
    };

    /** manages a system file containing logged output.
        the system file remains open during program execution. interfaces
        are provided for interoperating with standard log management
        tools like logrotate(8):
            http://linuxcommand.org/man_pages/logrotate8.html
        @note none of the listed interfaces are thread-safe.
    */
    class file
    {
    public:
        /** construct with no associated system file.
            a system file may be associated later with @ref open.
            @see open
        */
        file ();

        /** destroy the object.
            if a system file is associated, it will be flushed and closed.
        */
        ~file ();

        /** determine if a system file is associated with the log.
            @return `true` if a system file is associated and opened for writing.
        */
        bool isopen () const noexcept;

        /** associate a system file with the log.
            if the file does not exist an attempt is made to create it
            and open it for writing. if the file already exists an attempt is
            made to open it for appending.
            if a system file is already associated with the log, it is closed first.
            @return `true` if the file was opened.
        */
        // vfalco note the parameter is unfortunately a boost type because it
        //             can be either wchar or char based depending on platform.
        //        todo replace with beast::file
        //
        bool open (boost::filesystem::path const& path);

        /** close and re-open the system file associated with the log
            this assists in interoperating with external log management tools.
            @return `true` if the file was opened.
        */
        bool closeandreopen ();

        /** close the system file if it is open. */
        void close ();

        /** write to the log file.
            does nothing if there is no associated system file.
        */
        void write (char const* text);

        /** write to the log file and append an end of line marker.
            does nothing if there is no associated system file.
        */
        void writeln (char const* text);

        /** write to the log file using std::string. */
        /** @{ */
        void write (std::string const& str)
        {
            write (str.c_str ());
        }

        void writeln (std::string const& str)
        {
            writeln (str.c_str ());
        }
        /** @} */

    private:
        std::unique_ptr <std::ofstream> m_stream;
        boost::filesystem::path m_path;
    };

    std::mutex mutable mutex_;
    std::map <std::string, sink, beast::ci_less> sinks_;
    beast::journal::severity level_;
    file file_;

public:
    logs();

    logs (logs const&) = delete;
    logs& operator= (logs const&) = delete;

    bool
    open (boost::filesystem::path const& pathtologfile);

    sink&
    get (std::string const& name);

    sink&
    operator[] (std::string const& name);

    beast::journal
    journal (std::string const& name);

    beast::journal::severity
    severity() const;

    void
    severity (beast::journal::severity level);

    std::vector<std::pair<std::string, std::string>>
    partition_severities() const;

    void
    write (beast::journal::severity level, std::string const& partition,
        std::string const& text, bool console);

    std::string
    rotate();

public:
    static
    logseverity
    fromseverity (beast::journal::severity level);

    static
    beast::journal::severity
    toseverity (logseverity level);

    static
    std::string
    tostring (logseverity s);

    static
    logseverity
    fromstring (std::string const& s);

private:
    enum
    {
        // maximum line length for log messages.
        // if the message exceeds this length it will be truncated with elipses.
        maximummessagecharacters = 12 * 1024
    };

    static
    std::string
    scrub (std::string s);

    static
    void
    format (std::string& output, std::string const& message,
        beast::journal::severity severity, std::string const& partition);
};

//------------------------------------------------------------------------------
// vfalco deprecated temporary transition function until interfaces injected
inline
logs&
deprecatedlogs()
{
    static logs logs;
    return logs;
}
// vfalco deprecated inject beast::journal instead
#define shouldlog(s, k) deprecatedlogs()[#k].active(logs::toseverity(s))
#define writelog(s, k) if (!shouldlog (s, k)) do {} while (0); else \
    beast::journal::stream(deprecatedlogs()[#k], logs::toseverity(s))
#define condlog(c, s, k) if (!shouldlog (s, k) || !(c)) do {} while(0); else \
    beast::journal::stream(deprecatedlogs()[#k], logs::toseverity(s))

} // ripple

#endif
