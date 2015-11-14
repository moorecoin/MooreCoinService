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

#ifndef ripple_shamapaddnode_h
#define ripple_shamapaddnode_h

#include <beast/module/core/text/lexicalcast.h>

namespace ripple {

// results of adding nodes
class shamapaddnode
{
public:
    shamapaddnode ()
        : mgood (0)
        , mbad (0)
        , mduplicate (0)
    {
    }

    shamapaddnode (int good, int bad, int duplicate)
        : mgood(good)
        , mbad(bad)
        , mduplicate(duplicate)
    {
    }

    void incinvalid ()
    {
        ++mbad;
    }
    void incuseful ()
    {
        ++mgood;
    }
    void incduplicate ()
    {
        ++mduplicate;
    }

    void reset ()
    {
        mgood = mbad = mduplicate = 0;
    }

    int getgood ()
    {
        return mgood;
    }

    bool isinvalid () const
    {
        return mbad > 0;
    }
    bool isuseful () const
    {
        return mgood > 0;
    }

    shamapaddnode& operator+= (shamapaddnode const& n)
    {
        mgood += n.mgood;
        mbad += n.mbad;
        mduplicate += n.mduplicate;

        return *this;
    }

    bool isgood () const
    {
        return (mgood + mduplicate) > mbad;
    }

    static shamapaddnode duplicate ()
    {
        return shamapaddnode (0, 0, 1);
    }
    static shamapaddnode useful ()
    {
        return shamapaddnode (1, 0, 0);
    }
    static shamapaddnode invalid ()
    {
        return shamapaddnode (0, 1, 0);
    }

    std::string get ()
    {
        std::string ret;
        if (mgood > 0)
        {
            ret.append("good:");
            ret.append(beast::lexicalcastthrow<std::string>(mgood));
        }
        if (mbad > 0)
        {
            if (!ret.empty())
                ret.append(" ");
             ret.append("bad:");
             ret.append(beast::lexicalcastthrow<std::string>(mbad));
        }
        if (mduplicate > 0)
        {
            if (!ret.empty())
                ret.append(" ");
             ret.append("dupe:");
             ret.append(beast::lexicalcastthrow<std::string>(mduplicate));
        }
        if (ret.empty ())
            ret = "no nodes processed";
        return ret;
    }

private:

    int mgood;
    int mbad;
    int mduplicate;
};

}

#endif
