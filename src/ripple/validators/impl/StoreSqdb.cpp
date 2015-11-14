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
#include <ripple/validators/impl/storesqdb.h>
#include <beast/module/core/text/lexicalcast.h>
#include <beast/utility/debug.h>
#include <boost/regex.hpp>

namespace ripple {
namespace validators {

storesqdb::storesqdb (beast::journal journal)
    : m_journal (journal)
{
}

storesqdb::~storesqdb ()
{
}

beast::error
storesqdb::open (beast::file const& file)
{
    beast::error error (m_session.open (file.getfullpathname ()));

    m_journal.info <<
        "opening " << file.getfullpathname();

    if (error)
        m_journal.error <<
            "failed opening database: " << error.what();

    return error;
}

}
}
