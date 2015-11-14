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

#ifndef beast_semanticversion_h_included
#define beast_semanticversion_h_included

#include <vector>
#include <string>

#include <beast/utility/noexcept.h>

namespace beast {

/** a semantic version number.

    identifies the build of a particular version of software using
    the semantic versioning specification described here:

    http://semver.org/
*/
class semanticversion
{
public:
    typedef std::vector<std::string> identifier_list;

    int majorversion;
    int minorversion;
    int patchversion;

    identifier_list prereleaseidentifiers;
    identifier_list metadata;

    semanticversion ();

    semanticversion (std::string const& version);

    /** parse a semantic version string.
        the parsing is as strict as possible.
        @return `true` if the string was parsed.
    */
    bool parse (std::string const& input, bool debug = false);

    /** produce a string from semantic version components. */
    std::string print () const;

    inline bool isrelease () const noexcept
    { 
        return prereleaseidentifiers.empty();
    }
    inline bool isprerelease () const noexcept
    {
        return !isrelease ();
    }
};

/** compare two semanticversions against each other.
    the comparison follows the rules as per the specification.
*/
int compare (semanticversion const& lhs, semanticversion const& rhs);

inline bool
operator== (semanticversion const& lhs, semanticversion const& rhs) 
{ 
    return compare (lhs, rhs) == 0;
}

inline bool
operator!= (semanticversion const& lhs, semanticversion const& rhs)
{
    return compare (lhs, rhs) != 0;
}

inline bool
operator>= (semanticversion const& lhs, semanticversion const& rhs)
{
    return compare (lhs, rhs) >= 0;
}

inline bool
operator<= (semanticversion const& lhs, semanticversion const& rhs)
{
    return compare (lhs, rhs) <= 0;
}

inline bool
operator>  (semanticversion const& lhs, semanticversion const& rhs)
{
    return compare (lhs, rhs) >  0;
}

inline bool
operator<  (semanticversion const& lhs, semanticversion const& rhs)
{
    return compare (lhs, rhs) <  0;
}

} // beast

#endif
