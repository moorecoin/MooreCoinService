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

#ifndef ripple_protocol_ledgerformats_h_included
#define ripple_protocol_ledgerformats_h_included

#include <ripple/protocol/knownformats.h>

namespace ripple {

/** ledger entry types.

    these are stored in serialized data.

    @note changing these values results in a hard fork.

    @ingroup protocol
*/
// used as the type of a transaction or the type of a ledger entry.
enum ledgerentrytype
{
    ltinvalid           = -1,

    ltaccount_root      = 'a',

    /// describes an asset issuance
    ltasset             = 't',

    /// describes an asset holding
    ltasset_state       = 's',

    /** directory node.

        a directory is a vector 256-bit values. usually they represent
        hashes of other objects in the ledger.

        used in an append-only fashion.

        (there's a little more information than this, see the template)
    */
    ltdir_node          = 'd',
    
    ltdividend          = 'd',

    ltgenerator_map     = 'g',

    /// references hold by referee.
    ltrefer             = 'r',

    /** describes a trust line.
    */
    ltripple_state      = 'r',

    ltticket            = 't',

    /* deprecated. */
    ltoffer             = 'o',

    ltledger_hashes     = 'h',

    ltamendments        = 'f',

    ltfee_settings      = 's',

    // no longer used or supported. left here to prevent accidental
    // reassignment of the ledger type.
    ltnickname          = 'n',

    ltnotused01         = 'c',
};

/**
    @ingroup protocol
*/
// used as a prefix for computing ledger indexes (keys).
enum ledgernamespace
{
    spaceaccount        = 'a',
    spacedirnode        = 'd',
    spacegenerator      = 'g',
    spaceripple         = 'r',
    spaceoffer          = 'o',  // entry for an offer.
    spaceownerdir       = 'o',  // directory of things owned by an account.
    spacebookdir        = 'b',  // directory of order books.
    spacecontract       = 'c',
    spaceskiplist       = 's',
    spaceamendment      = 'f',
    spacefee            = 'e',
    spaceticket         = 't',
    spacedividend       = 'd',
    spacerefer          = 'r',
    spaceasset          = 't',
    spaceassetstate     = 's',

    // no longer used or supported. left here to reserve the space and
    // avoid accidental reuse of the space.
    spacenickname       = 'n',
};

/**
    @ingroup protocol
*/
enum ledgerspecificflags
{
    // ltaccount_root
    lsfpasswordspent    = 0x00010000,   // true, if password set fee is spent.
    lsfrequiredesttag   = 0x00020000,   // true, to require a destinationtag for payments.
    lsfrequireauth      = 0x00040000,   // true, to require a authorization to hold ious.
    lsfdisallowxrp      = 0x00080000,   // true, to disallow sending xrp.
    lsfdisablemaster    = 0x00100000,   // true, force regular key
    lsfnofreeze         = 0x00200000,   // true, cannot freeze ripple states
    lsfglobalfreeze     = 0x00400000,   // true, all assets frozen
	lsfdisallowvbc		= 0x00800000,   // true, to disallow sending vbc.

    // ltoffer
    lsfpassive          = 0x00010000,
    lsfsell             = 0x00020000,   // true, offer was placed as a sell.

    // ltripple_state
    lsflowreserve       = 0x00010000,   // true, if entry counts toward reserve.
    lsfhighreserve      = 0x00020000,
    lsflowauth          = 0x00040000,
    lsfhighauth         = 0x00080000,
    lsflownoripple      = 0x00100000,
    lsfhighnoripple     = 0x00200000,
    lsflowfreeze        = 0x00400000,   // true, low side has set freeze flag
    lsfhighfreeze       = 0x00800000,   // true, high side has set freeze flag
};

//------------------------------------------------------------------------------

/** holds the list of known ledger entry formats.
*/
class ledgerformats : public knownformats <ledgerentrytype>
{
private:
    ledgerformats ();

public:
    static ledgerformats const& getinstance ();

private:
    void addcommonfields (item& item);
};

} // ripple

#endif
