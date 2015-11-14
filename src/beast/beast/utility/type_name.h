//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, howard hinnant <howard.hinnant@gmail.com>

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

#ifndef beast_utility_type_name_h_included
#define beast_utility_type_name_h_included

#include <type_traits>
#include <typeinfo>
#include <iostream>
#ifndef _msc_ver
#include <cxxabi.h>
#endif
#include <memory>
#include <string>
#include <cstdlib>
#include <vector>

namespace beast {

#ifdef _msc_ver
#pragma warning (push)
#pragma warning (disable: 4127) // conditional expression is constant
#endif

template <typename t>
std::string
type_name()
{
    typedef typename std::remove_reference<t>::type tr;
    std::unique_ptr<char, void(*)(void*)> own (
    #ifndef _msc_ver
        abi::__cxa_demangle (typeid(tr).name(), nullptr,
            nullptr, nullptr),
    #else
            nullptr,
    #endif
            std::free
    );
    std::string r = own != nullptr ? own.get() : typeid(tr).name();
    if (std::is_const<tr>::value)
        r += " const";
    if (std::is_volatile<tr>::value)
        r += " volatile";
    if (std::is_lvalue_reference<t>::value)
        r += "&";
    else if (std::is_rvalue_reference<t>::value)
        r += "&&";
    return r;
}

#ifdef _msc_ver
#pragma warning (pop)
#endif

} // beast

#endif
