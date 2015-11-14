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
#include <ripple/rpc/impl/jsonobject.h>

namespace ripple {
namespace rpc {

collection::collection (collection* parent, writer* writer)
        : parent_ (parent), writer_ (writer), enabled_ (true)
{
    checkwritable ("collection::collection()");
    if (parent_)
    {
        check (parent_->enabled_, "parent not enabled in constructor");
        parent_->enabled_ = false;
    }
}

collection::~collection ()
{
    if (writer_)
        writer_->finish ();
    if (parent_)
        parent_->enabled_ = true;
}

collection& collection::operator= (collection&& that)
{
    parent_ = that.parent_;
    writer_ = that.writer_;
    enabled_ = that.enabled_;

    that.parent_ = nullptr;
    that.writer_ = nullptr;
    that.enabled_ = false;

    return *this;
}

collection::collection (collection&& that)
{
    *this = std::move (that);
}

void collection::checkwritable (std::string const& label)
{
    if (!enabled_)
        throw jsonexception (label + ": not enabled");
    if (!writer_)
        throw jsonexception (label + ": not writable");
}

//------------------------------------------------------------------------------

object::root::root (writer& w) : object (nullptr, &w)
{
    writer_->startroot (writer::object);
}

object object::makeobject (std::string const& key)
{
    checkwritable ("object::makeobject");
    if (writer_)
        writer_->startset (writer::object, key);
    return object (this, writer_);
}

array object::makearray (std::string const& key) {
    checkwritable ("object::makearray");
    if (writer_)
        writer_->startset (writer::array, key);
    return array (this, writer_);
}

//------------------------------------------------------------------------------

object array::makeobject ()
{
    checkwritable ("array::makeobject");
    if (writer_)
        writer_->startappend (writer::object);
    return object (this, writer_);
}

array array::makearray ()
{
    checkwritable ("array::makearray");
    if (writer_)
        writer_->startappend (writer::array);
    return array (this, writer_);
}

//------------------------------------------------------------------------------

object::proxy::proxy (object& object, std::string const& key)
    : object_ (object)
    , key_ (key)
{
}

object::proxy object::operator[] (std::string const& key)
{
    return proxy (*this, key);
}

object::proxy object::operator[] (json::staticstring const& key)
{
    return proxy (*this, std::string (key));
}

namespace {

template <class object>
void docopyfrom (object& to, json::value const& from)
{
    auto members = from.getmembernames();
    for (auto& m: members)
        to[m] = from[m];
}

}

void copyfrom (json::value& to, json::value const& from)
{
    if (to.empty())  // short circuit this very common case.
        to = from;
    else
        docopyfrom (to, from);
}

void copyfrom (object& to, json::value const& from)
{
    docopyfrom (to, from);
}


writerobject stringwriterobject (std::string& s)
{
    return writerobject (stringoutput (s));
}

} // rpc
} // ripple
