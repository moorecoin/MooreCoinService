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

#ifndef ripple_protocol_txformats_h_included
#define ripple_protocol_txformats_h_included

#include <ripple/protocol/knownformats.h>

namespace ripple {

/** transaction type identifiers.

    these are part of the binary message format.

    @ingroup protocol
*/
enum txtype
{
    ttinvalid           = -1,

    ttpayment           = 0,
    ttclaim             = 1, // open
    ttwallet_add        = 2,
    ttaccount_set       = 3,
    ttpassword_fund     = 4, // open
    ttregular_key_set   = 5,
    ttnickname_set      = 6, // open
    ttoffer_create      = 7,
    ttoffer_cancel      = 8,
    no_longer_used      = 9,
    ttticket_create     = 10,
    ttticket_cancel     = 11,

    tttrust_set         = 20,

    ttamendment         = 100,
    ttfee               = 101,
    
    ttdividend          = 181,
    ttaddreferee        = 182,
    ttactiveaccount     = 183,
    ttissue             = 184,
};

/** manages the list of known transaction formats.
*/
class txformats : public knownformats <txtype>
{
private:
    void addcommonfields (item& item);

public:
    /** create the object.
        this will load the object will all the known transaction formats.
    */
    txformats ();

    static txformats const& getinstance ();
};

} // ripple

#endif
