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

#ifndef ripple_validations_h_included
#define ripple_validations_h_included

#include <ripple/protocol/stvalidation.h>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

// vfalco todo rename and move these typedefs into the validations interface

// nodes validating and highest node id validating
typedef hash_map<nodeid, stvalidation::pointer> validationset;

typedef std::pair<int, nodeid> validationcounter;
typedef hash_map<uint256, validationcounter> ledgertovalidationcounter;
typedef std::vector<stvalidation::pointer> validationvector;

class validations : beast::leakchecked <validations>
{
public:

    virtual ~validations () { }

    virtual bool addvalidation (stvalidation::ref, std::string const& source) = 0;

    virtual validationset getvalidations (uint256 const& ledger) = 0;

    virtual void getvalidationcount (
        uint256 const& ledger, bool currentonly, int& trusted,
        int& untrusted) = 0;
    virtual void getvalidationtypes (
        uint256 const& ledger, int& full, int& partial) = 0;

    virtual int gettrustedvalidationcount (uint256 const& ledger) = 0;

    /** returns fees reported by trusted validators in the given ledger. */
    virtual
    std::vector <std::uint64_t>
    fees (uint256 const& ledger, std::uint64_t base) = 0;

    virtual int getnodesafter (uint256 const& ledger) = 0;
    virtual int getloadratio (bool overloaded) = 0;

    // vfalco todo make a typedef for this ugly return value!
    virtual ledgertovalidationcounter getcurrentvalidations (
        uint256 currentledger, uint256 previousledger) = 0;

    virtual std::list <stvalidation::pointer>
    getcurrenttrustedvalidations () = 0;

    virtual void tune (int size, int age) = 0;

    virtual void flush () = 0;

    virtual void sweep () = 0;
};

std::unique_ptr <validations> make_validations ();

} // ripple

#endif
