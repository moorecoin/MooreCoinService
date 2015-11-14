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

#ifndef ripple_clusternodestatus_h_included
#define ripple_clusternodestatus_h_included

namespace ripple {

class clusternodestatus
{
public:

    clusternodestatus() : mloadfee(0), mreporttime(0)
    { ; }

    explicit clusternodestatus(std::string const& name) : mnodename(name), mloadfee(0), mreporttime(0)
    { ; }

    clusternodestatus(std::string const& name, std::uint32_t fee, std::uint32_t rtime) :
        mnodename(name),
        mloadfee(fee),
        mreporttime(rtime)
    { ; }

    std::string const& getname()
    {
        return mnodename;
    }

    std::uint32_t getloadfee()
    {
        return mloadfee;
    }

    std::uint32_t getreporttime()
    {
        return mreporttime;
    }

    bool update(clusternodestatus const& status)
    {
        if (status.mreporttime <= mreporttime)
            return false;
        mloadfee = status.mloadfee;
        mreporttime = status.mreporttime;
        if (mnodename.empty() || !status.mnodename.empty())
            mnodename = status.mnodename;
        return true;
    }

private:
    std::string       mnodename;
    std::uint32_t     mloadfee;
    std::uint32_t     mreporttime;
};

} // ripple

#endif
