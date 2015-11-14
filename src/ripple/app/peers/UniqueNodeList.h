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

#ifndef ripple_uniquenodelist_h_included
#define ripple_uniquenodelist_h_included

#include <ripple/app/peers/clusternodestatus.h>
#include <beast/cxx14/memory.h> // <memory>
#include <beast/threads/stoppable.h>
#include <boost/filesystem.hpp>

namespace ripple {

class uniquenodelist : public beast::stoppable
{
protected:
    explicit uniquenodelist (stoppable& parent);

public:
    enum validatorsource
    {
        vsconfig    = 'c',  // rippled.cfg
        vsinbound   = 'i',
        vsmanual    = 'm',
        vsreferral  = 'r',
        vstold      = 't',
        vsvalidator = 'v',  // validators.txt
        vsweb       = 'w',
    };

    // vfalco todo rename this to use the right coding style
    typedef long score;

public:
    virtual ~uniquenodelist () { }

    // vfalco todo roll this into the constructor so there is one less state.
    virtual void start () = 0;

    // vfalco todo rename all these, the "node" prefix is redundant (lol)
    virtual void nodeaddpublic (rippleaddress const& nanodepublic, validatorsource vswhy, std::string const& strcomment) = 0;
    virtual void nodeadddomain (std::string strdomain, validatorsource vswhy, std::string const& strcomment = "") = 0;
    virtual void noderemovepublic (rippleaddress const& nanodepublic) = 0;
    virtual void noderemovedomain (std::string strdomain) = 0;
    virtual void nodereset () = 0;

    virtual void nodescore () = 0;

    virtual bool nodeinunl (rippleaddress const& nanodepublic) = 0;
    virtual bool nodeincluster (rippleaddress const& nanodepublic) = 0;
    virtual bool nodeincluster (rippleaddress const& nanodepublic, std::string& name) = 0;
    virtual bool nodeupdate (rippleaddress const& nanodepublic, clusternodestatus const& cnsstatus) = 0;
    virtual std::map<rippleaddress, clusternodestatus> getclusterstatus () = 0;
    virtual std::uint32_t getclusterfee () = 0;
    virtual void addclusterstatus (json::value&) = 0;

    virtual void nodebootstrap () = 0;
    virtual bool nodeload (boost::filesystem::path pconfig) = 0;
    virtual void nodenetwork () = 0;

    virtual json::value getunljson () = 0;

    virtual int isourcescore (validatorsource vswhy) = 0;
};

std::unique_ptr<uniquenodelist>
make_uniquenodelist (beast::stoppable& parent);

} // ripple

#endif
