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

//  ratio_io
//
//  (c) copyright howard hinnant
//  use, modification and distribution are subject to the boost software license,
//  version 1.0. (see accompanying file license_1_0.txt or copy at
//  http://www.boost.org/license_1_0.txt).

#ifndef beast_chrono_ratio_io_h_included
#define beast_chrono_ratio_io_h_included

/*

    ratio_io synopsis

#include <ratio>
#include <string>

namespace std
{

template <class ratio, class chart>
struct ratio_string
{
    static basic_string<chart> symbol();
    static basic_string<chart> prefix();
};

}  // std

*/

#include <ratio>
#include <string>
#include <sstream>

//_libcpp_begin_namespace_std
namespace std {

template <class _ratio, class _chart>
struct ratio_string
{
    static basic_string<_chart> symbol() {return prefix();}
    static basic_string<_chart> prefix();
};

template <class _ratio, class _chart>
basic_string<_chart>
ratio_string<_ratio, _chart>::prefix()
{
    basic_ostringstream<_chart> __os;
    __os << _chart('[') << _ratio::num << _chart('/')
                        << _ratio::den << _chart(']');
    return __os.str();
}

// atto

template <>
struct ratio_string<atto, char>
{
    static string symbol() {return string(1, 'a');}
    static string prefix()  {return string("atto");}
};

#if has_unicode_support

template <>
struct ratio_string<atto, char16_t>
{
    static u16string symbol() {return u16string(1, u'a');}
    static u16string prefix()  {return u16string(u"atto");}
};

template <>
struct ratio_string<atto, char32_t>
{
    static u32string symbol() {return u32string(1, u'a');}
    static u32string prefix()  {return u32string(u"atto");}
};

#endif

template <>
struct ratio_string<atto, wchar_t>
{
    static wstring symbol() {return wstring(1, l'a');}
    static wstring prefix()  {return wstring(l"atto");}
};

// femto

template <>
struct ratio_string<femto, char>
{
    static string symbol() {return string(1, 'f');}
    static string prefix()  {return string("femto");}
};

#if has_unicode_support

template <>
struct ratio_string<femto, char16_t>
{
    static u16string symbol() {return u16string(1, u'f');}
    static u16string prefix()  {return u16string(u"femto");}
};

template <>
struct ratio_string<femto, char32_t>
{
    static u32string symbol() {return u32string(1, u'f');}
    static u32string prefix()  {return u32string(u"femto");}
};

#endif

template <>
struct ratio_string<femto, wchar_t>
{
    static wstring symbol() {return wstring(1, l'f');}
    static wstring prefix()  {return wstring(l"femto");}
};

// pico

template <>
struct ratio_string<pico, char>
{
    static string symbol() {return string(1, 'p');}
    static string prefix()  {return string("pico");}
};

#if has_unicode_support

template <>
struct ratio_string<pico, char16_t>
{
    static u16string symbol() {return u16string(1, u'p');}
    static u16string prefix()  {return u16string(u"pico");}
};

template <>
struct ratio_string<pico, char32_t>
{
    static u32string symbol() {return u32string(1, u'p');}
    static u32string prefix()  {return u32string(u"pico");}
};

#endif

template <>
struct ratio_string<pico, wchar_t>
{
    static wstring symbol() {return wstring(1, l'p');}
    static wstring prefix()  {return wstring(l"pico");}
};

// nano

template <>
struct ratio_string<nano, char>
{
    static string symbol() {return string(1, 'n');}
    static string prefix()  {return string("nano");}
};

#if has_unicode_support

template <>
struct ratio_string<nano, char16_t>
{
    static u16string symbol() {return u16string(1, u'n');}
    static u16string prefix()  {return u16string(u"nano");}
};

template <>
struct ratio_string<nano, char32_t>
{
    static u32string symbol() {return u32string(1, u'n');}
    static u32string prefix()  {return u32string(u"nano");}
};

#endif

template <>
struct ratio_string<nano, wchar_t>
{
    static wstring symbol() {return wstring(1, l'n');}
    static wstring prefix()  {return wstring(l"nano");}
};

// micro

template <>
struct ratio_string<micro, char>
{
    static string symbol() {return string("\xc2\xb5");}
    static string prefix()  {return string("micro");}
};

#if has_unicode_support

template <>
struct ratio_string<micro, char16_t>
{
    static u16string symbol() {return u16string(1, u'\xb5');}
    static u16string prefix()  {return u16string(u"micro");}
};

template <>
struct ratio_string<micro, char32_t>
{
    static u32string symbol() {return u32string(1, u'\xb5');}
    static u32string prefix()  {return u32string(u"micro");}
};

#endif

template <>
struct ratio_string<micro, wchar_t>
{
    static wstring symbol() {return wstring(1, l'\xb5');}
    static wstring prefix()  {return wstring(l"micro");}
};

// milli

template <>
struct ratio_string<milli, char>
{
    static string symbol() {return string(1, 'm');}
    static string prefix()  {return string("milli");}
};

#if has_unicode_support

template <>
struct ratio_string<milli, char16_t>
{
    static u16string symbol() {return u16string(1, u'm');}
    static u16string prefix()  {return u16string(u"milli");}
};

template <>
struct ratio_string<milli, char32_t>
{
    static u32string symbol() {return u32string(1, u'm');}
    static u32string prefix()  {return u32string(u"milli");}
};

#endif

template <>
struct ratio_string<milli, wchar_t>
{
    static wstring symbol() {return wstring(1, l'm');}
    static wstring prefix()  {return wstring(l"milli");}
};

// centi

template <>
struct ratio_string<centi, char>
{
    static string symbol() {return string(1, 'c');}
    static string prefix()  {return string("centi");}
};

#if has_unicode_support

template <>
struct ratio_string<centi, char16_t>
{
    static u16string symbol() {return u16string(1, u'c');}
    static u16string prefix()  {return u16string(u"centi");}
};

template <>
struct ratio_string<centi, char32_t>
{
    static u32string symbol() {return u32string(1, u'c');}
    static u32string prefix()  {return u32string(u"centi");}
};

#endif

template <>
struct ratio_string<centi, wchar_t>
{
    static wstring symbol() {return wstring(1, l'c');}
    static wstring prefix()  {return wstring(l"centi");}
};

// deci

template <>
struct ratio_string<deci, char>
{
    static string symbol() {return string(1, 'd');}
    static string prefix()  {return string("deci");}
};

#if has_unicode_support

template <>
struct ratio_string<deci, char16_t>
{
    static u16string symbol() {return u16string(1, u'd');}
    static u16string prefix()  {return u16string(u"deci");}
};

template <>
struct ratio_string<deci, char32_t>
{
    static u32string symbol() {return u32string(1, u'd');}
    static u32string prefix()  {return u32string(u"deci");}
};

#endif

template <>
struct ratio_string<deci, wchar_t>
{
    static wstring symbol() {return wstring(1, l'd');}
    static wstring prefix()  {return wstring(l"deci");}
};

// deca

template <>
struct ratio_string<deca, char>
{
    static string symbol() {return string("da");}
    static string prefix()  {return string("deca");}
};

#if has_unicode_support

template <>
struct ratio_string<deca, char16_t>
{
    static u16string symbol() {return u16string(u"da");}
    static u16string prefix()  {return u16string(u"deca");}
};

template <>
struct ratio_string<deca, char32_t>
{
    static u32string symbol() {return u32string(u"da");}
    static u32string prefix()  {return u32string(u"deca");}
};

#endif

template <>
struct ratio_string<deca, wchar_t>
{
    static wstring symbol() {return wstring(l"da");}
    static wstring prefix()  {return wstring(l"deca");}
};

// hecto

template <>
struct ratio_string<hecto, char>
{
    static string symbol() {return string(1, 'h');}
    static string prefix()  {return string("hecto");}
};

#if has_unicode_support

template <>
struct ratio_string<hecto, char16_t>
{
    static u16string symbol() {return u16string(1, u'h');}
    static u16string prefix()  {return u16string(u"hecto");}
};

template <>
struct ratio_string<hecto, char32_t>
{
    static u32string symbol() {return u32string(1, u'h');}
    static u32string prefix()  {return u32string(u"hecto");}
};

#endif

template <>
struct ratio_string<hecto, wchar_t>
{
    static wstring symbol() {return wstring(1, l'h');}
    static wstring prefix()  {return wstring(l"hecto");}
};

// kilo

template <>
struct ratio_string<kilo, char>
{
    static string symbol() {return string(1, 'k');}
    static string prefix()  {return string("kilo");}
};

#if has_unicode_support

template <>
struct ratio_string<kilo, char16_t>
{
    static u16string symbol() {return u16string(1, u'k');}
    static u16string prefix()  {return u16string(u"kilo");}
};

template <>
struct ratio_string<kilo, char32_t>
{
    static u32string symbol() {return u32string(1, u'k');}
    static u32string prefix()  {return u32string(u"kilo");}
};

#endif

template <>
struct ratio_string<kilo, wchar_t>
{
    static wstring symbol() {return wstring(1, l'k');}
    static wstring prefix()  {return wstring(l"kilo");}
};

// mega

template <>
struct ratio_string<mega, char>
{
    static string symbol() {return string(1, 'm');}
    static string prefix()  {return string("mega");}
};

#if has_unicode_support

template <>
struct ratio_string<mega, char16_t>
{
    static u16string symbol() {return u16string(1, u'm');}
    static u16string prefix()  {return u16string(u"mega");}
};

template <>
struct ratio_string<mega, char32_t>
{
    static u32string symbol() {return u32string(1, u'm');}
    static u32string prefix()  {return u32string(u"mega");}
};

#endif

template <>
struct ratio_string<mega, wchar_t>
{
    static wstring symbol() {return wstring(1, l'm');}
    static wstring prefix()  {return wstring(l"mega");}
};

// giga

template <>
struct ratio_string<giga, char>
{
    static string symbol() {return string(1, 'g');}
    static string prefix()  {return string("giga");}
};

#if has_unicode_support

template <>
struct ratio_string<giga, char16_t>
{
    static u16string symbol() {return u16string(1, u'g');}
    static u16string prefix()  {return u16string(u"giga");}
};

template <>
struct ratio_string<giga, char32_t>
{
    static u32string symbol() {return u32string(1, u'g');}
    static u32string prefix()  {return u32string(u"giga");}
};

#endif

template <>
struct ratio_string<giga, wchar_t>
{
    static wstring symbol() {return wstring(1, l'g');}
    static wstring prefix()  {return wstring(l"giga");}
};

// tera

template <>
struct ratio_string<tera, char>
{
    static string symbol() {return string(1, 't');}
    static string prefix()  {return string("tera");}
};

#if has_unicode_support

template <>
struct ratio_string<tera, char16_t>
{
    static u16string symbol() {return u16string(1, u't');}
    static u16string prefix()  {return u16string(u"tera");}
};

template <>
struct ratio_string<tera, char32_t>
{
    static u32string symbol() {return u32string(1, u't');}
    static u32string prefix()  {return u32string(u"tera");}
};

#endif

template <>
struct ratio_string<tera, wchar_t>
{
    static wstring symbol() {return wstring(1, l't');}
    static wstring prefix()  {return wstring(l"tera");}
};

// peta

template <>
struct ratio_string<peta, char>
{
    static string symbol() {return string(1, 'p');}
    static string prefix()  {return string("peta");}
};

#if has_unicode_support

template <>
struct ratio_string<peta, char16_t>
{
    static u16string symbol() {return u16string(1, u'p');}
    static u16string prefix()  {return u16string(u"peta");}
};

template <>
struct ratio_string<peta, char32_t>
{
    static u32string symbol() {return u32string(1, u'p');}
    static u32string prefix()  {return u32string(u"peta");}
};

#endif

template <>
struct ratio_string<peta, wchar_t>
{
    static wstring symbol() {return wstring(1, l'p');}
    static wstring prefix()  {return wstring(l"peta");}
};

// exa

template <>
struct ratio_string<exa, char>
{
    static string symbol() {return string(1, 'e');}
    static string prefix()  {return string("exa");}
};

#if has_unicode_support

template <>
struct ratio_string<exa, char16_t>
{
    static u16string symbol() {return u16string(1, u'e');}
    static u16string prefix()  {return u16string(u"exa");}
};

template <>
struct ratio_string<exa, char32_t>
{
    static u32string symbol() {return u32string(1, u'e');}
    static u32string prefix()  {return u32string(u"exa");}
};

#endif

template <>
struct ratio_string<exa, wchar_t>
{
    static wstring symbol() {return wstring(1, l'e');}
    static wstring prefix()  {return wstring(l"exa");}
};

//_libcpp_end_namespace_std
}

#endif  // _ratio_io
