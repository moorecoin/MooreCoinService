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

#ifndef ripple_loadfeetrackimp_h_included
#define ripple_loadfeetrackimp_h_included

#include <ripple/protocol/jsonfields.h>
#include <ripple/core/loadfeetrack.h>
#include <mutex>

namespace ripple {

class loadfeetrackimp : public loadfeetrack
{
public:
    explicit loadfeetrackimp (beast::journal journal = beast::journal())
        : m_journal (journal)
        , mlocaltxnloadfee (lftnormalfee)
        , mremotetxnloadfee (lftnormalfee)
        , mclustertxnloadfee (lftnormalfee)
        , raisecount (0)
    {
    }

    // scale using load as well as base rate
    std::uint64_t scalefeeload (std::uint64_t fee, std::uint64_t basefee, std::uint32_t referencefeeunits, bool badmin)
    {
        static std::uint64_t midrange (0x00000000ffffffff);

        bool big = (fee > midrange);

        if (big)                // big fee, divide first to avoid overflow
            fee /= referencefeeunits;
        else                    // normal fee, multiply first for accuracy
            fee *= basefee;

        std::uint32_t feefactor = std::max (mlocaltxnloadfee, mremotetxnloadfee);

        // let admins pay the normal fee until the local load exceeds four times the remote
        std::uint32_t uremfee = std::max(mremotetxnloadfee, mclustertxnloadfee);
        if (badmin && (feefactor > uremfee) && (feefactor < (4 * uremfee)))
            feefactor = uremfee;

        {
            scopedlocktype sl (mlock);
            fee = muldiv (fee, feefactor, lftnormalfee);
        }

        if (big)                // fee was big to start, must now multiply
            fee *= basefee;
        else                    // fee was small to start, mst now divide
            fee /= referencefeeunits;

        return fee;
    }

    // scale from fee units to millionths of a ripple
    std::uint64_t scalefeebase (std::uint64_t fee, std::uint64_t basefee, std::uint32_t referencefeeunits)
    {
        return muldiv (fee, basefee, referencefeeunits);
    }

    std::uint32_t getremotefee ()
    {
        scopedlocktype sl (mlock);
        return mremotetxnloadfee;
    }

    std::uint32_t getlocalfee ()
    {
        scopedlocktype sl (mlock);
        return mlocaltxnloadfee;
    }

    std::uint32_t getloadbase ()
    {
        return lftnormalfee;
    }

    std::uint32_t getloadfactor ()
    {
        scopedlocktype sl (mlock);
        return std::max(mclustertxnloadfee, std::max (mlocaltxnloadfee, mremotetxnloadfee));
    }

    void setclusterfee (std::uint32_t fee)
    {
        scopedlocktype sl (mlock);
        mclustertxnloadfee = fee;
    }

    std::uint32_t getclusterfee ()
    {
        scopedlocktype sl (mlock);
        return mclustertxnloadfee;
    }

    bool isloadedlocal ()
    {
        // vfalco todo this could be replaced with a shareddata and
        //             using a read/write lock instead of a critical section.
        //
        //        note this applies to all the locking in this class.
        //
        //
        scopedlocktype sl (mlock);
        return (raisecount != 0) || (mlocaltxnloadfee != lftnormalfee);
    }

    bool isloadedcluster ()
    {
        // vfalco todo this could be replaced with a shareddata and
        //             using a read/write lock instead of a critical section.
        //
        //        note this applies to all the locking in this class.
        //
        //
        scopedlocktype sl (mlock);
        return (raisecount != 0) || (mlocaltxnloadfee != lftnormalfee) || (mclustertxnloadfee != lftnormalfee);
    }

    void setremotefee (std::uint32_t f)
    {
        scopedlocktype sl (mlock);
        mremotetxnloadfee = f;
    }

    bool raiselocalfee ()
    {
        scopedlocktype sl (mlock);

        if (++raisecount < 2)
            return false;

        std::uint32_t origfee = mlocaltxnloadfee;

        if (mlocaltxnloadfee < mremotetxnloadfee) // make sure this fee takes effect
            mlocaltxnloadfee = mremotetxnloadfee;

        mlocaltxnloadfee += (mlocaltxnloadfee / lftfeeincfraction); // increment by 1/16th

        if (mlocaltxnloadfee > lftfeemax)
            mlocaltxnloadfee = lftfeemax;

        if (origfee == mlocaltxnloadfee)
            return false;

        m_journal.debug << "local load fee raised from " << origfee << " to " << mlocaltxnloadfee;
        return true;
    }

    bool lowerlocalfee ()
    {
        scopedlocktype sl (mlock);
        std::uint32_t origfee = mlocaltxnloadfee;
        raisecount = 0;

        mlocaltxnloadfee -= (mlocaltxnloadfee / lftfeedecfraction ); // reduce by 1/4

        if (mlocaltxnloadfee < lftnormalfee)
            mlocaltxnloadfee = lftnormalfee;

        if (origfee == mlocaltxnloadfee)
            return false;

        m_journal.debug << "local load fee lowered from " << origfee << " to " << mlocaltxnloadfee;
        return true;
    }

    json::value getjson (std::uint64_t basefee, std::uint32_t referencefeeunits)
    {
        json::value j (json::objectvalue);

        {
            scopedlocktype sl (mlock);

            // base_fee = the cost to send a "reference" transaction under no load, in millionths of a ripple
            j[jss::base_fee] = json::value::uint (basefee);

            // load_fee = the cost to send a "reference" transaction now, in millionths of a ripple
            j[jss::load_fee] = json::value::uint (
                                muldiv (basefee, std::max (mlocaltxnloadfee, mremotetxnloadfee), lftnormalfee));
        }

        return j;
    }

private:
    // vfalco todo move this function to some "math utilities" file
    // compute (value)*(mul)/(div) - avoid overflow but keep precision
    std::uint64_t muldiv (std::uint64_t value, std::uint32_t mul, std::uint64_t div)
    {
        // vfalco todo replace with beast::literal64bitunsigned ()
        //
        static std::uint64_t boundary = (0x00000000ffffffff);

        if (value > boundary)                           // large value, avoid overflow
            return (value / div) * mul;
        else                                            // normal value, preserve accuracy
            return (value * mul) / div;
    }

private:
    static const int lftnormalfee = 256;        // 256 is the minimum/normal load factor
    static const int lftfeeincfraction = 4;     // increase fee by 1/4
    static const int lftfeedecfraction = 4;     // decrease fee by 1/4
    static const int lftfeemax = lftnormalfee * 1000000;

    beast::journal m_journal;
    typedef std::mutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype mlock;

    std::uint32_t mlocaltxnloadfee;        // scale factor, lftnormalfee = normal fee
    std::uint32_t mremotetxnloadfee;       // scale factor, lftnormalfee = normal fee
    std::uint32_t mclustertxnloadfee;      // scale factor, lftnormalfee = normal fee
    int raisecount;
};

} // ripple

#endif
