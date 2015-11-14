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

#ifndef ripple_validators_storesqdb_h_included
#define ripple_validators_storesqdb_h_included

#include <ripple/validators/impl/store.h>
#include <beast/module/core/files/file.h>
#include <beast/module/sqdb/sqdb.h>
#include <beast/utility/error.h>
#include <beast/utility/journal.h>

namespace ripple {
namespace validators {

/** database persistence for validators using sqlite */
class storesqdb : public store
{
private:
    beast::journal m_journal;
    beast::sqdb::session m_session;

public:
    enum
    {
        // this affects the format of the data!
        currentschemaversion = 1
    };

    explicit
    storesqdb (beast::journal journal);

    ~storesqdb();

    beast::error
    open (beast::file const& file);
};

}
}

#endif
