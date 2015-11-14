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
#include <ripple/rpc/status.h>

namespace ripple {
namespace rpc {

std::string status::codestring () const
{
    if (!*this)
        return "";

    if (type_ == type::none)
        return std::to_string (code_);

    if (type_ == status::type::ter)
    {
        std::string s1, s2;

        auto success = transresultinfo (toter (), s1, s2);
        assert (success);
        (void) success;

        return s1 + ": " + s2;
    }

    if (type_ == status::type::error_code_i)
    {
        auto info = get_error_info (toerrorcode ());
        return info.token +  ": " + info.message;
    }

    assert (false);
    return "";
}

void status::filljson (json::value& value)
{
    static const std::string separator = ": ";

    if (!*this)
        return;

    auto& error = value[jss::error];
    error[jss::code] = code_;
    error[jss::message] = codestring ();

    // are there any more messages?
    if (!messages_.empty ())
    {
        auto& messages = error[jss::data];
        for (auto& i: messages_)
            messages.append (i);
    }
}

std::string status::message() const {
    std::string result;
    for (auto& m: messages_)
    {
        if (!result.empty())
            result += '/';
        result += m;
    }

    return result;
}

std::string status::tostring() const {
    if (*this)
        return codestring() + ":" + message();
    return "";
}

} // namespace rpc
} // ripple
