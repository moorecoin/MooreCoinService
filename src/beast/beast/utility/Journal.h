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

#ifndef beast_utility_journal_h_included
#define beast_utility_journal_h_included

#include <sstream>

namespace beast {

/** a generic endpoint for log messages. */
class journal
{
public:
    /** severity level of the message. */
    enum severity
    {
        kall = 0,

        ktrace = kall,
        kdebug,
        kinfo,
        kwarning,
        kerror,
        kfatal,

        kdisabled,
        knone = kdisabled
    };

    class sink;

private:
    journal& operator= (journal const& other); // disallowed

    sink* m_sink;
    severity m_level;

public:
    //--------------------------------------------------------------------------

    /** abstraction for the underlying message destination. */
    class sink
    {
    public:
        sink ();

        virtual ~sink () = 0;

        /** returns `true` if text at the passed severity produces output. */
        virtual bool active (severity level) const;

        /** returns `true` if a message is also written to the output window (msvc). */
        virtual bool console () const;

        /** set whether messages are also written to the output window (msvc). */
        virtual void console (bool output);

        /** returns the minimum severity level this sink will report. */
        virtual severity severity() const;

        /** set the minimum severity this sink will report. */
        virtual void severity (severity level);

        /** write text to the sink at the specified severity.
            the caller is responsible for checking the minimum severity level
            before using this function.
        */
        virtual void write (severity level, std::string const& text) = 0;

    private:
        severity m_level;
        bool m_console;
    };

    /** returns a sink which does nothing. */
    static sink& getnullsink ();

    //--------------------------------------------------------------------------

    class stream;

    /** scoped ostream-based container for writing messages to a journal. */
    class scopedstream
    {
    public:
        explicit scopedstream (stream const& stream);
        scopedstream (scopedstream const& other);

        template <typename t>
        scopedstream (stream const& stream, t const& t)
            : m_sink (stream.sink())
            , m_level (stream.severity())
            , m_active (stream.active ())
        {
            if (active ())
                m_ostream << t;
        }

        scopedstream (stream const& stream,
            std::ostream& manip (std::ostream&));

        ~scopedstream ();

        bool active () const
            { return m_active; }

        std::ostringstream& ostream () const;

        std::ostream& operator<< (
            std::ostream& manip (std::ostream&)) const;

        template <typename t>
        std::ostream& operator<< (t const& t) const
        {
            if (active ())
                m_ostream << t;
            return m_ostream;
        }

    private:
        void init ();

        scopedstream& operator= (scopedstream const&); // disallowed

        sink& m_sink;
        severity const m_level;
        bool const m_active;
        std::ostringstream mutable m_ostream;
        bool m_tooutputwindow;
    };

    //--------------------------------------------------------------------------

    class stream
    {
    public:
        /** create a stream which produces no output. */
        stream ();

        /** create stream that writes at the given level. */
        /** @{ */
        stream (sink& sink, severity level, bool active = true);
        stream (stream const& stream, bool active);
        /** @} */

        /** construct or copy another stream. */
        /** @{ */
        stream (stream const& other);
        stream& operator= (stream const& other);
        /** @} */

        /** returns the sink that this stream writes to. */
        sink& sink() const;

        /** returns the severity of messages this stream reports. */
        severity severity() const;

        /** returns `true` if sink logs anything at this stream's severity. */
        /** @{ */
        bool active() const;
        
        explicit
        operator bool() const
        {
            return active();
        }
        /** @} */

        /** output stream support. */
        /** @{ */
        scopedstream operator<< (std::ostream& manip (std::ostream&)) const;

        template <typename t>
        scopedstream operator<< (t const& t) const
        {
            return scopedstream (*this, t);
        }
        /** @} */

    private:
        sink* m_sink;
        severity m_level;
        bool m_disabled;
    };

    //--------------------------------------------------------------------------

    /** create a journal that writes to the null sink. */
    journal ();

    /** create a journal that writes to the specified sink. */
    /** @{ */
    explicit journal (sink& sink, severity level = kall);

    /** create a journal from another journal.
        when specifying a new minimum severity level, the effective minimum
        level will be the higher of the other journal and the specified value.
    */
    /** @{ */
    journal (journal const& other);
    journal (journal const& other, severity level);
    /** @} */

    /** destroy the journal. */
    ~journal ();

    /** returns the sink associated with this journal. */
    sink& sink() const;

    /** returns a stream for this sink, with the specified severity. */
    stream stream (severity level) const;

    /** returns `true` if any message would be logged at this severity level.
        for a message to be logged, the severity must be at or above both
        the journal's severity level and the sink's severity level.
    */
    bool active (severity level) const;

    /** returns this journal's minimum severity level.
        if the underlying sink has a higher threshold, there will still
        be no output at that level.
    */
    severity severity () const;

    /** convenience sink streams for each severity level. */
    stream const trace;
    stream const debug;
    stream const info;
    stream const warning;
    stream const error;
    stream const fatal;
};

}

#endif
