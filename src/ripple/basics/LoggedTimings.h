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

#ifndef ripple_basics_loggedtimings_h_included
#define ripple_basics_loggedtimings_h_included

#include <ripple/basics/log.h>
#include <beast/module/core/time/time.h>
#include <beast/module/core/diagnostic/measurefunctioncalltime.h>
#include <beast/utility/debug.h>
#include <ripple/basics/syncunorderedmap.h>

namespace ripple {

namespace detail {

/** template class that performs destruction of an object.
    default implementation simply calls delete
*/
template <typename object>
struct destroyer;

/** specialization for std::shared_ptr.
*/
template <typename object>
struct destroyer <std::shared_ptr <object> >
{
    static void destroy (std::shared_ptr <object>& p)
    {
        p.reset ();
    }
};

/** specialization for std::unordered_map
*/
template <typename key, typename value, typename hash, typename alloc>
struct destroyer <std::unordered_map <key, value, hash, alloc> >
{
    static void destroy (std::unordered_map <key, value, hash, alloc>& v)
    {
        v.clear ();
    }
};

/** specialization for syncunorderedmaptype
*/
template <typename key, typename value, typename hash>
struct destroyer <syncunorderedmaptype <key, value, hash> >
{
    static void destroy (syncunorderedmaptype <key, value, hash>& v)
    {
        v.clear ();
    }
};

/** cleans up an elaspsed time so it prints nicely */
inline double cleanelapsed (double seconds) noexcept
{
    if (seconds >= 10)
        return std::floor (seconds + 0.5);

    return static_cast <int> ((seconds * 10 + 0.5) / 10);
}

} // detail

//------------------------------------------------------------------------------

/** measure the time required to destroy an object.
*/

template <typename object>
double timeddestroy (object& object)
{
    std::int64_t const starttime (beast::time::gethighresolutionticks ());

    detail::destroyer <object>::destroy (object);

    return beast::time::highresolutiontickstoseconds (
            beast::time::gethighresolutionticks () - starttime);
}

/** log the timed destruction of an object if the time exceeds a threshold.
*/
template <typename partitionkey, typename object>
void logtimeddestroy (
    object& object,
    std::string const& objectdescription,
    double thresholdseconds = 1)
{
    double const seconds = timeddestroy (object);

    if (seconds > thresholdseconds)
    {
        deprecatedlogs().journal("loggedtimings").warning <<
            objectdescription << " took " <<
            detail::cleanelapsed (seconds) <<
            " seconds to destroy";
    }
}

//------------------------------------------------------------------------------

/** log a timed function call if the time exceeds a threshold. */
template <typename function>
void logtimedcall (
    beast::journal::stream stream,
    std::string const& description,
    char const* filename,
    int linenumber,
    function f,
    double thresholdseconds = 1)
{
    double const seconds = beast::measurefunctioncalltime (f);

    if (seconds > thresholdseconds)
    {
        stream <<
            description << " took "<<
            detail::cleanelapsed (seconds) <<
            " seconds to execute at " <<
            beast::debug::getsourcelocation (filename, linenumber);
    }
}

} // ripple

#endif
