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
#include <ripple/app/misc/ihashrouter.h>
#include <ripple/basics/countedobject.h>
#include <ripple/basics/unorderedcontainers.h>
#include <ripple/basics/uptimetimer.h>
#include <boost/foreach.hpp>
#include <map>
#include <mutex>

namespace ripple {

// vfalco todo inline the function definitions
class hashrouter : public ihashrouter
{
private:
    /** an entry in the routing table.
    */
    class entry : public countedobject <entry>
    {
    public:
        static char const* getcountedobjectname () { return "hashrouterentry"; }

        entry ()
            : mflags (0)
        {
        }

        std::set <peershortid> const& peekpeers () const
        {
            return mpeers;
        }

        void addpeer (peershortid peer)
        {
            if (peer != 0)
                mpeers.insert (peer);
        }

        bool haspeer (peershortid peer) const
        {
            return mpeers.count (peer) > 0;
        }

        int getflags (void) const
        {
            return mflags;
        }

        bool hasflag (int mask) const
        {
            return (mflags & mask) != 0;
        }

        void setflag (int flagstoset)
        {
            mflags |= flagstoset;
        }

        void clearflag (int flagstoclear)
        {
            mflags &= ~flagstoclear;
        }

        void swapset (std::set <peershortid>& other)
        {
            mpeers.swap (other);
        }

    private:
        int mflags;
        std::set <peershortid> mpeers;
    };

public:
    explicit hashrouter (int holdtime)
        : mholdtime (holdtime)
    {
    }

    bool addsuppression (uint256 const& index);

    bool addsuppressionpeer (uint256 const& index, peershortid peer);
    bool addsuppressionpeer (uint256 const& index, peershortid peer, int& flags);
    bool addsuppressionflags (uint256 const& index, int flag);
    bool setflag (uint256 const& index, int flag);
    int getflags (uint256 const& index);

    bool swapset (uint256 const& index, std::set<peershortid>& peers, int flag);

private:
    entry getentry (uint256 const& );

    entry& findcreateentry (uint256 const& , bool& created);

    using locktype = std::mutex;
    using scopedlocktype = std::lock_guard <locktype>;
    locktype mlock;

    // stores all suppressed hashes and their expiration time
    hash_map <uint256, entry> msuppressionmap;

    // stores all expiration times and the hashes indexed for them
    std::map< int, std::list<uint256> > msuppressiontimes;

    int mholdtime;
};

//------------------------------------------------------------------------------

hashrouter::entry& hashrouter::findcreateentry (uint256 const& index, bool& created)
{
    hash_map<uint256, entry>::iterator fit = msuppressionmap.find (index);

    if (fit != msuppressionmap.end ())
    {
        created = false;
        return fit->second;
    }

    created = true;

    int now = uptimetimer::getinstance ().getelapsedseconds ();
    int expiretime = now - mholdtime;

    // see if any supressions need to be expired
    std::map< int, std::list<uint256> >::iterator it = msuppressiontimes.begin ();

    if ((it != msuppressiontimes.end ()) && (it->first <= expiretime))
    {
        boost_foreach (uint256 const& lit, it->second)
        msuppressionmap.erase (lit);
        msuppressiontimes.erase (it);
    }

    msuppressiontimes[now].push_back (index);
    return msuppressionmap.emplace (index, entry ()).first->second;
}

bool hashrouter::addsuppression (uint256 const& index)
{
    scopedlocktype sl (mlock);

    bool created;
    findcreateentry (index, created);
    return created;
}

hashrouter::entry hashrouter::getentry (uint256 const& index)
{
    scopedlocktype sl (mlock);

    bool created;
    return findcreateentry (index, created);
}

bool hashrouter::addsuppressionpeer (uint256 const& index, peershortid peer)
{
    scopedlocktype sl (mlock);

    bool created;
    findcreateentry (index, created).addpeer (peer);
    return created;
}

bool hashrouter::addsuppressionpeer (uint256 const& index, peershortid peer, int& flags)
{
    scopedlocktype sl (mlock);

    bool created;
    entry& s = findcreateentry (index, created);
    s.addpeer (peer);
    flags = s.getflags ();
    return created;
}

int hashrouter::getflags (uint256 const& index)
{
    scopedlocktype sl (mlock);

    bool created;
    return findcreateentry (index, created).getflags ();
}

bool hashrouter::addsuppressionflags (uint256 const& index, int flag)
{
    scopedlocktype sl (mlock);

    bool created;
    findcreateentry (index, created).setflag (flag);
    return created;
}

bool hashrouter::setflag (uint256 const& index, int flag)
{
    // vfalco note comments like this belong in the header file,
    //             and more importantly in a javadoc comment so
    //             they appear in the generated documentation.
    //
    // return: true = changed, false = unchanged
    assert (flag != 0);

    scopedlocktype sl (mlock);

    bool created;
    entry& s = findcreateentry (index, created);

    if ((s.getflags () & flag) == flag)
        return false;

    s.setflag (flag);
    return true;
}

bool hashrouter::swapset (uint256 const& index, std::set<peershortid>& peers, int flag)
{
    scopedlocktype sl (mlock);

    bool created;
    entry& s = findcreateentry (index, created);

    if ((s.getflags () & flag) == flag)
        return false;

    s.swapset (peers);
    s.setflag (flag);

    return true;
}

ihashrouter* ihashrouter::new (int holdtime)
{
    return new hashrouter (holdtime);
}

} // ripple
