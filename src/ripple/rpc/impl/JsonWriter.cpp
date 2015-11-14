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
#include <ripple/rpc/impl/jsonwriter.h>
#include <ripple/rpc/impl/writejson.h>
#include <beast/unit_test/suite.h>

namespace ripple {
namespace rpc {

namespace {

std::map <char, const char*> jsonspecialcharacterescape = {
    {'"',  "\\\""},
    {'\\', "\\\\"},
    {'/',  "\\/"},
    {'\b', "\\b"},
    {'\f', "\\f"},
    {'\n', "\\n"},
    {'\r', "\\r"},
    {'\t', "\\t"}
};

static size_t const jsonescapelength = 2;

// all other json punctuation.
const char closebrace = '}';
const char closebracket = ']';
const char colon = ':';
const char comma = ',';
const char openbrace = '{';
const char openbracket = '[';
const char quote = '"';

const std::string none;

static auto const integralfloatsbecomeints = false;

size_t lengthwithouttrailingzeros (std::string const& s)
{
    auto dotpos = s.find ('.');
    if (dotpos == std::string::npos)
        return s.size();

    auto lastnonzero = s.find_last_not_of ('0');
    auto hasdecimals = dotpos != lastnonzero;

    if (hasdecimals)
        return lastnonzero + 1;

    if (integralfloatsbecomeints || lastnonzero + 2 > s.size())
        return lastnonzero;

    return lastnonzero + 2;
}

} // namespace

class writer::impl
{
public:
    impl (output const& output) : output_(output) {}
    ~impl() = default;

    impl(impl&&) = delete;
    impl& operator=(impl&&) = delete;

    bool empty() const { return stack_.empty (); }

    void start (collectiontype ct)
    {
        char ch = (ct == array) ? openbracket : openbrace;
        output ({&ch, 1});
        stack_.push (collection());
        stack_.top().type = ct;
    }

    void output (boost::string_ref const& bytes)
    {
        markstarted ();
        output_ (bytes);
    }

    void stringoutput (boost::string_ref const& bytes)
    {
        markstarted ();
        std::size_t position = 0, writtenuntil = 0;

        output_ ({&quote, 1});
        auto data = bytes.data();
        for (; position < bytes.size(); ++position)
        {
            auto i = jsonspecialcharacterescape.find (data[position]);
            if (i != jsonspecialcharacterescape.end ())
            {
                if (writtenuntil < position)
                {
                    output_ ({data + writtenuntil, position - writtenuntil});
                }
                output_ ({i->second, jsonescapelength});
                writtenuntil = position + 1;
            };
        }
        if (writtenuntil < position)
            output_ ({data + writtenuntil, position - writtenuntil});
        output_ ({&quote, 1});
    }

    void markstarted ()
    {
        check (!isfinished(), "isfinished() in output.");
        isstarted_ = true;
    }

    void nextcollectionentry (collectiontype type, std::string const& message)
    {
        check (!empty() , "empty () in " + message);

        auto t = stack_.top ().type;
        if (t != type)
        {
            check (false, "not an " +
                   ((type == array ? "array: " : "object: ") + message));
        }
        if (stack_.top ().isfirst)
            stack_.top ().isfirst = false;
        else
            output_ ({&comma, 1});
    }

    void writeobjecttag (std::string const& tag)
    {
#ifdef debug
        // make sure we haven't already seen this tag.
        auto& tags = stack_.top ().tags;
        check (tags.find (tag) == tags.end (), "already seen tag " + tag);
        tags.insert (tag);
#endif

        stringoutput (tag);
        output_ ({&colon, 1});
    }

    bool isfinished() const
    {
        return isstarted_ && empty();
    }

    void finish ()
    {
        check (!empty(), "empty stack in finish()");

        auto isarray = stack_.top().type == array;
        auto ch = isarray ? closebracket : closebrace;
        output_ ({&ch, 1});
        stack_.pop();
    }

    void finishall ()
    {
        if (isstarted_)
        {
            while (!isfinished())
                finish();
        }
    }

    output const& getoutput() const { return output_; }

private:
    // json collections are either arrrays, or objects.
    struct collection
    {
        /** what type of collection are we in? */
        writer::collectiontype type;

        /** is this the first entry in a collection?
         *  if false, we have to emit a , before we write the next entry. */
        bool isfirst = true;

#ifdef debug
        /** what tags have we already seen in this collection? */
        std::set <std::string> tags;
#endif
    };

    using stack = std::stack <collection, std::vector<collection>>;

    output output_;
    stack stack_;

    bool isstarted_ = false;
};

writer::writer (output const &output)
        : impl_(std::make_unique <impl> (output))
{
}

writer::~writer()
{
    if (impl_)
        impl_->finishall ();
}

writer::writer(writer&& w)
{
    impl_ = std::move (w.impl_);
}

writer& writer::operator=(writer&& w)
{
    impl_ = std::move (w.impl_);
    return *this;
}

void writer::output (char const* s)
{
    impl_->stringoutput (s);
}

void writer::output (std::string const& s)
{
    impl_->stringoutput (s);
}

void writer::output (json::value const& value)
{
    impl_->markstarted();
    writejson (value, impl_->getoutput());
}

template <>
void writer::output (float f)
{
    auto s = to_string (f);
    impl_->output ({s.data (), lengthwithouttrailingzeros (s)});
}

template <>
void writer::output (double f)
{
    auto s = to_string (f);
    impl_->output ({s.data (), lengthwithouttrailingzeros (s)});
}

template <>
void writer::output (std::nullptr_t)
{
    impl_->output ("null");
}

void writer::imploutput (std::string const& s)
{
    impl_->output (s);
}

void writer::finishall ()
{
    if (impl_)
        impl_->finishall ();
}

void writer::rawappend()
{
    impl_->nextcollectionentry (array, "append");
}

void writer::rawset (std::string const& tag)
{
    check (!tag.empty(), "tag can't be empty");

    impl_->nextcollectionentry (object, "set");
    impl_->writeobjecttag (tag);
}

void writer::startroot (collectiontype type)
{
    impl_->start (type);
}

void writer::startappend (collectiontype type)
{
    impl_->nextcollectionentry (array, "startappend");
    impl_->start (type);
}

void writer::startset (collectiontype type, std::string const& key)
{
    impl_->nextcollectionentry (object, "startset");
    impl_->writeobjecttag (key);
    impl_->start (type);
}

void writer::finish ()
{
    if (impl_)
        impl_->finish ();
}

} // rpc
} // ripple
