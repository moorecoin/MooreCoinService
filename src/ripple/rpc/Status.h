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

#ifndef ripple_status_h
#define ripple_status_h

#include <ripple/protocol/ter.h>
#include <ripple/protocol/errorcodes.h>

namespace ripple {
namespace rpc {

/** status represents the results of an operation that might fail.

    it wraps the legacy codes ter and error_code_i, providing both a uniform
    interface and a way to attach additional information to existing status
    returns.

    a status can also be used to fill a json::value with a json-rpc 2.0
    error response:  see http://www.jsonrpc.org/specification#error_object
 */
struct status
{
public:
    enum class type {none, ter, error_code_i};
    using code = int;
    using strings = std::vector <std::string>;

    static const code ok = 0;

    status () = default;

    status (code code, strings d = {})
            : type_ (type::none), code_ (code), messages_ (std::move (d))
    {
    }

    status (ter ter, strings d = {})
            : type_ (type::ter), code_ (ter), messages_ (std::move (d))
    {
    }

    status (error_code_i e, strings d = {})
            : type_ (type::error_code_i), code_ (e), messages_ (std::move (d))
    {
    }

    status (error_code_i e, std::string const& s)
            : type_ (type::error_code_i), code_ (e), messages_ ({s})
    {
    }

    /* returns a representation of the integer status code as a string.
       if the status is ok, the result is an empty string.
    */
    std::string codestring () const;

    /** returns true if the status is *not* ok. */
    operator bool() const
    {
        return code_ != ok;
    }

    /** returns true if the status is ok. */
    bool operator !() const
    {
        return ! bool (*this);
    }

    /** returns the status as a ter.
        this may only be called if type() == type::ter. */
    ter toter () const
    {
        assert (type_ == type::ter);
        return ter (code_);
    }

    /** returns the status as an error_code_i.
        this may only be called if type() == type::error_code_i. */
    error_code_i toerrorcode() const
    {
        assert (type_ == type::error_code_i);
        return error_code_i (code_);
    }

    /** apply the status to a jsonobject
     */
    template <class object>
    void inject (object& object)
    {
        if (auto ec = toerrorcode())
        {
            if (messages_.empty())
                inject_error (ec, object);
            else
                inject_error (ec, message(), object);
        }
    }

    strings const& messages() const
    {
        return messages_;
    }

    /** return the first message, if any. */
    std::string message() const;

    type type() const
    {
        return type_;
    }

    std::string tostring() const;

    /** fill a json::value with an rpc 2.0 response.
        if the status is ok, filljson has no effect.
        not currently used. */
    void filljson(json::value&);

private:
    type type_ = type::none;
    code code_ = ok;
    strings messages_;
};

} // namespace rpc
} // ripple

#endif
