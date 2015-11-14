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

#ifndef ripple_peerfinder_store_h_included
#define ripple_peerfinder_store_h_included

namespace ripple {
namespace peerfinder {

/** abstract persistence for peerfinder data. */
class store
{
public:
    virtual ~store () { }

    // load the bootstrap cache
    typedef std::function <void (beast::ip::endpoint, int)> load_callback;
    virtual std::size_t load (load_callback const& cb) = 0;

    // save the bootstrap cache
    struct entry
    {
        beast::ip::endpoint endpoint;
        int valence;
    };
    virtual void save (std::vector <entry> const& v) = 0;
};

}
}

#endif
