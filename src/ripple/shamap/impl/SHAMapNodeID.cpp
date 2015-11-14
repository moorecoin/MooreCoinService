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
#include <ripple/shamap/shamapnodeid.h>
#include <ripple/crypto/randomnumbers.h>
#include <beast/module/core/text/lexicalcast.h>
#include <beast/utility/static_initializer.h>
#include <boost/format.hpp>
#include <cassert>
#include <cstring>

namespace ripple {

uint256 const&
shamapnodeid::masks (int depth)
{
    enum
    {
        mask_size = 65
    };

    struct masks_t
    {
        uint256 entry [mask_size];

        masks_t()
        {
            uint256 selector;
            for (int i = 0; i < mask_size-1; i += 2)
            {
                entry[i] = selector;
                * (selector.begin () + (i / 2)) = 0xf0;
                entry[i + 1] = selector;
                * (selector.begin () + (i / 2)) = 0xff;
            }
            entry[mask_size-1] = selector;
        }
    };
    static beast::static_initializer <masks_t> masks;
    return masks->entry[depth];
}

std::size_t
shamapnodeid::calculate_hash (uint256 const& node, int depth)
{
    struct hashparams
    {
        hashparams ()
            : golden_ratio (0x9e3779b9)
        {
            random_fill (&cookie_value);
        }

        // the cookie value protects us against algorithmic complexity attacks.
        std::size_t cookie_value;
        std::size_t golden_ratio;
    };

    static beast::static_initializer <hashparams> params;

    std::size_t h = params->cookie_value + (depth * params->golden_ratio);

    auto ptr = reinterpret_cast <const unsigned int*> (node.cbegin ());

    for (int i = (depth + 7) / 8; i != 0; --i)
        h = (h * params->golden_ratio) ^ *ptr++;

    return h;
}

// canonicalize the hash to a node id for this depth
shamapnodeid::shamapnodeid (int depth, uint256 const& hash)
    : mnodeid (hash), mdepth (depth), mhash (0)
{
    assert ((depth >= 0) && (depth < 65));
    mnodeid &= masks(depth);
}

shamapnodeid::shamapnodeid (void const* ptr, int len) : mhash (0)
{
    if (len < 33)
        mdepth = -1;
    else
    {
        std::memcpy (mnodeid.begin (), ptr, 32);
        mdepth = * (static_cast<unsigned char const*> (ptr) + 32);
    }
}

std::string shamapnodeid::getstring () const
{
    if ((mdepth == 0) && (mnodeid.iszero ()))
        return "nodeid(root)";

    return "nodeid(" + std::to_string (mdepth) +
        "," + to_string (mnodeid) + ")";
}

uint256 shamapnodeid::getnodeid (int depth, uint256 const& hash)
{
    assert ((depth >= 0) && (depth <= 64));
    return hash & masks(depth);
}

void shamapnodeid::addidraw (serializer& s) const
{
    s.add256 (mnodeid);
    s.add8 (mdepth);
}

std::string shamapnodeid::getrawstring () const
{
    serializer s (33);
    addidraw (s);
    return s.getstring ();
}

// this can be optimized to avoid the << if needed
shamapnodeid shamapnodeid::getchildnodeid (int m) const
{
    assert ((m >= 0) && (m < 16));
    assert (mdepth <= 64);

    uint256 child (mnodeid);
    child.begin ()[mdepth / 2] |= (mdepth & 1) ? m : (m << 4);

    return shamapnodeid (mdepth + 1, child, true);
}

// which branch would contain the specified hash
int shamapnodeid::selectbranch (uint256 const& hash) const
{
#if ripple_verify_nodeobject_keys

    if (mdepth >= 64)
    {
        assert (false);
        return -1;
    }

    if ((hash & masks(mdepth)) != mnodeid)
    {
        std::cerr << "selectbranch(" << getstring () << std::endl;
        std::cerr << "  " << hash << " off branch" << std::endl;
        assert (false);
        return -1;  // does not go under this node
    }

#endif

    int branch = * (hash.begin () + (mdepth / 2));

    if (mdepth & 1)
        branch &= 0xf;
    else
        branch >>= 4;

    assert ((branch >= 0) && (branch < 16));

    return branch;
}

void shamapnodeid::dump (beast::journal journal) const
{
    if (journal.debug) journal.debug <<
        getstring ();
}

} // ripple
