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
#include <ripple/shamap/shamap.h>
    
namespace ripple {

// this code is used to compare another node's transaction tree
// to our own. it returns a map containing all items that are different
// between two sha maps. it is optimized not to descend down tree
// branches with the same branch hash. a limit can be passed so
// that we will abort early if a node sends a map to us that
// makes no sense at all. (and our sync algorithm will avoid
// synchronizing matching branches too.)

bool shamap::walkbranch (shamaptreenode* node,
                         shamapitem::ref othermapitem, bool isfirstmap,
                         delta& differences, int& maxcount)
{
    // walk a branch of a shamap that's matched by an empty branch or single item in the other map
    std::stack <shamaptreenode*, std::vector<shamaptreenode*>> nodestack;
    nodestack.push ({node});

    bool emptybranch = !othermapitem;

    while (!nodestack.empty ())
    {
        node = nodestack.top ();
        nodestack.pop ();

        if (node->isinner ())
        {
            // this is an inner node, add all non-empty branches
            for (int i = 0; i < 16; ++i)
                if (!node->isemptybranch (i))
                    nodestack.push ({descendthrow (node, i)});
        }
        else
        {
            // this is a leaf node, process its item
            shamapitem::pointer item = node->peekitem ();

            if (emptybranch || (item->gettag () != othermapitem->gettag ()))
            {
                // unmatched
                if (isfirstmap)
                    differences.insert (std::make_pair (item->gettag (),
                                      deltaref (item, shamapitem::pointer ())));
                else
                    differences.insert (std::make_pair (item->gettag (),
                                      deltaref (shamapitem::pointer (), item)));

                if (--maxcount <= 0)
                    return false;
            }
            else if (item->peekdata () != othermapitem->peekdata ())
            {
                // non-matching items with same tag
                if (isfirstmap)
                    differences.insert (std::make_pair (item->gettag (),
                                                deltaref (item, othermapitem)));
                else
                    differences.insert (std::make_pair (item->gettag (),
                                                deltaref (othermapitem, item)));

                if (--maxcount <= 0)
                    return false;

                emptybranch = true;
            }
            else
            {
                // exact match
                emptybranch = true;
            }
        }
    }

    if (!emptybranch)
    {
        // othermapitem was unmatched, must add
        if (isfirstmap) // this is first map, so other item is from second
            differences.insert (std::make_pair (othermapitem->gettag (),
                                                deltaref (shamapitem::pointer(),
                                                          othermapitem)));
        else
            differences.insert (std::make_pair (othermapitem->gettag (),
                                                deltaref (othermapitem,
                                                      shamapitem::pointer ())));

        if (--maxcount <= 0)
            return false;
    }

    return true;
}

bool shamap::compare (shamap::ref othermap, delta& differences, int maxcount)
{
    // compare two hash trees, add up to maxcount differences to the difference table
    // return value: true=complete table of differences given, false=too many differences
    // throws on corrupt tables or missing nodes
    // caution: othermap is not locked and must be immutable

    assert (isvalid () && othermap && othermap->isvalid ());

    using stackentry = std::pair <shamaptreenode*, shamaptreenode*>;
    std::stack <stackentry, std::vector<stackentry>> nodestack; // track nodes we've pushed

    if (gethash () == othermap->gethash ())
        return true;

    nodestack.push ({root.get(), othermap->root.get()});
    while (!nodestack.empty ())
    {
        shamaptreenode* ournode = nodestack.top().first;
        shamaptreenode* othernode = nodestack.top().second;
        nodestack.pop ();

        if (!ournode || !othernode)
        {
            assert (false);
            throw shamapmissingnode (mtype, uint256 ());
        }

        if (ournode->isleaf () && othernode->isleaf ())
        {
            // two leaves
            if (ournode->gettag () == othernode->gettag ())
            {
                if (ournode->peekdata () != othernode->peekdata ())
                {
                    differences.insert (std::make_pair (ournode->gettag (),
                                                 deltaref (ournode->peekitem (),
                                                 othernode->peekitem ())));
                    if (--maxcount <= 0)
                        return false;
                }
            }
            else
            {
                differences.insert (std::make_pair(ournode->gettag (),
                                                   deltaref(ournode->peekitem(),
                                                   shamapitem::pointer ())));
                if (--maxcount <= 0)
                    return false;

                differences.insert(std::make_pair(othernode->gettag (),
                                                  deltaref(shamapitem::pointer(),
                                                  othernode->peekitem ())));
                if (--maxcount <= 0)
                    return false;
            }
        }
        else if (ournode->isinner () && othernode->isleaf ())
        {
            if (!walkbranch (ournode, othernode->peekitem (),
                    true, differences, maxcount))
                return false;
        }
        else if (ournode->isleaf () && othernode->isinner ())
        {
            if (!othermap->walkbranch (othernode, ournode->peekitem (),
                                       false, differences, maxcount))
                return false;
        }
        else if (ournode->isinner () && othernode->isinner ())
        {
            for (int i = 0; i < 16; ++i)
                if (ournode->getchildhash (i) != othernode->getchildhash (i))
                {
                    if (othernode->isemptybranch (i))
                    {
                        // we have a branch, the other tree does not
                        shamaptreenode* inode = descendthrow (ournode, i);
                        if (!walkbranch (inode,
                                         shamapitem::pointer (), true,
                                         differences, maxcount))
                            return false;
                    }
                    else if (ournode->isemptybranch (i))
                    {
                        // the other tree has a branch, we do not
                        shamaptreenode* inode =
                            othermap->descendthrow(othernode, i);
                        if (!othermap->walkbranch (inode,
                                                   shamapitem::pointer(),
                                                   false, differences, maxcount))
                            return false;
                    }
                    else // the two trees have different non-empty branches
                        nodestack.push ({descendthrow (ournode, i),
                                        othermap->descendthrow (othernode, i)});
                }
        }
        else
            assert (false);
    }

    return true;
}

void shamap::walkmap (std::vector<shamapmissingnode>& missingnodes, int maxmissing)
{
    std::stack <shamaptreenode::pointer,
        std::vector <shamaptreenode::pointer>> nodestack;

    if (!root->isinner ())  // root is only node, and we have it
        return;

    nodestack.push (root);

    while (!nodestack.empty ())
    {
        shamaptreenode::pointer node = std::move (nodestack.top());
        nodestack.pop ();

        for (int i = 0; i < 16; ++i)
        {
            if (!node->isemptybranch (i))
            {
                shamaptreenode::pointer nextnode = descendnostore (node, i);

                if (nextnode)
                {
                    if (nextnode->isinner ())
                        nodestack.push (std::move (nextnode));
                }
                else
                {
                    missingnodes.emplace_back (mtype, node->getchildhash (i));
                    if (--maxmissing <= 0)
                        return;
                }
            }
        }
    }
}

} // ripple
