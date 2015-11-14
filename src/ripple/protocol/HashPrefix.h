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

#ifndef ripple_protocol_hashprefix_h_included
#define ripple_protocol_hashprefix_h_included

#include <cstdint>

namespace ripple {

/** prefix for hashing functions.

    these prefixes are inserted before the source material used to generate
    various hashes. this is done to put each hash in its own "space." this way,
    two different types of objects with the same binary data will produce
    different hashes.

    each prefix is a 4-byte value with the last byte set to zero and the first
    three bytes formed from the ascii equivalent of some arbitrary string. for
    example "txn".

    @note hash prefixes are part of the ripple protocol.

    @ingroup protocol
*/
class hashprefix
{
private:
    std::uint32_t m_prefix;

    hashprefix (char a, char b, char c)
        : m_prefix (0)
    {
        m_prefix = a;
        m_prefix = (m_prefix << 8) + b;
        m_prefix = (m_prefix << 8) + c;
        m_prefix = m_prefix << 8;
    }

public:
    hashprefix(hashprefix const&) = delete;
    hashprefix& operator=(hashprefix const&) = delete;

    /** returns the hash prefix associated with this object */
    operator std::uint32_t () const
    {
        return m_prefix;
    }

    // vfalco todo expand the description to complete, concise sentences.
    //

    /** transaction plus signature to give transaction id */
    static hashprefix const transactionid;

    /** transaction plus metadata */
    static hashprefix const txnode;

    /** account state */
    static hashprefix const leafnode;

    /** inner node in tree */
    static hashprefix const innernode;

    /** ledger master data for signing */
    static hashprefix const ledgermaster;

    /** inner transaction to sign */
    static hashprefix const txsign;

    /** validation for signing */
    static hashprefix const validation;

    /** proposal for signing */
    static hashprefix const proposal;
};

} // ripple

#endif
