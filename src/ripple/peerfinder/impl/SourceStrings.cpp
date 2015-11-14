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
#include <ripple/peerfinder/impl/sourcestrings.h>

namespace ripple {
namespace peerfinder {

class sourcestringsimp : public sourcestrings
{
public:
    sourcestringsimp (std::string const& name, strings const& strings)
        : m_name (name)
        , m_strings (strings)
    {
    }

    ~sourcestringsimp ()
    {
    }

    std::string const& name ()
    {
        return m_name;
    }

    void fetch (results& results, beast::journal journal)
    {
        results.addresses.resize (0);
        results.addresses.reserve (m_strings.size());
        for (int i = 0; i < m_strings.size (); ++i)
        {
            beast::ip::endpoint ep (beast::ip::endpoint::from_string (m_strings [i]));
            if (is_unspecified (ep))
                ep = beast::ip::endpoint::from_string_altform (m_strings [i]);
            if (! is_unspecified (ep))
                results.addresses.push_back (ep);
        }
    }

private:
    std::string m_name;
    strings m_strings;
};

//------------------------------------------------------------------------------

beast::sharedptr <source>
sourcestrings::new (std::string const& name, strings const& strings)
{
    return new sourcestringsimp (name, strings);
}

}
}
