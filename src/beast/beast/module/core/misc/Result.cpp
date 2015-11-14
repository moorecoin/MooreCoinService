//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

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

namespace beast
{

result::result() noexcept {}

result::result (const string& message) noexcept
    : errormessage (message)
{
}

result::result (const result& other)
    : errormessage (other.errormessage)
{
}

result& result::operator= (const result& other)
{
    errormessage = other.errormessage;
    return *this;
}

#if beast_compiler_supports_move_semantics
result::result (result&& other) noexcept
    : errormessage (static_cast <string&&> (other.errormessage))
{
}

result& result::operator= (result&& other) noexcept
{
    errormessage = static_cast <string&&> (other.errormessage);
    return *this;
}
#endif

bool result::operator== (const result& other) const noexcept
{
    return errormessage == other.errormessage;
}

bool result::operator!= (const result& other) const noexcept
{
    return errormessage != other.errormessage;
}

result result::fail (const string& errormessage) noexcept
{
    return result (errormessage.isempty() ? "unknown error" : errormessage);
}

const string& result::geterrormessage() const noexcept
{
    return errormessage;
}

bool result::wasok() const noexcept         { return errormessage.isempty(); }
result::operator bool() const noexcept      { return errormessage.isempty(); }
bool result::failed() const noexcept        { return errormessage.isnotempty(); }
bool result::operator!() const noexcept     { return errormessage.isnotempty(); }

} // beast
