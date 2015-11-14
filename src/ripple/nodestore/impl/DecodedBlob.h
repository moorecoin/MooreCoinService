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

#ifndef ripple_nodestore_decodedblob_h_included
#define ripple_nodestore_decodedblob_h_included

#include <ripple/nodestore/nodeobject.h>

namespace ripple {
namespace nodestore {

/** parsed key/value blob into nodeobject components.

    this will extract the information required to construct a nodeobject. it
    also does consistency checking and returns the result, so it is possible
    to determine if the data is corrupted without throwing an exception. not
    all forms of corruption are detected so further analysis will be needed
    to eliminate false negatives.

    @note this defines the database format of a nodeobject!
*/
class decodedblob
{
public:
    /** construct the decoded blob from raw data. */
    decodedblob (void const* key, void const* value, int valuebytes);

    /** determine if the decoding was successful. */
    bool wasok () const noexcept { return m_success; }

    /** create a nodeobject from this data. */
    nodeobject::ptr createobject ();

private:
    bool m_success;

    void const* m_key;
    nodeobjecttype m_objecttype;
    unsigned char const* m_objectdata;
    int m_databytes;
};

}
}

#endif
