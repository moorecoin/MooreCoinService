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
#include <ripple/basics/log.h>
#include <ripple/protocol/hashprefix.h>
#include <ripple/protocol/protocol.h>
#include <ripple/protocol/staccount.h>
#include <ripple/protocol/starray.h>
#include <ripple/protocol/sttx.h>
#include <ripple/protocol/stparsedjson.h>
#include <ripple/protocol/txflags.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/json/to_string.h>
#include <beast/unit_test/suite.h>
#include <boost/format.hpp>

namespace ripple {

sttx::sttx (txtype type)
    : stobject (sftransaction)
    , tx_type_ (type)
    , sig_state_ (boost::indeterminate)
{
    auto format = txformats::getinstance().findbytype (type);

    if (format == nullptr)
    {
        writelog (lswarning, sttx) <<
            "transaction type: " << type;
        throw std::runtime_error ("invalid transaction type");
    }

    set (format->elements);
    setfieldu16 (sftransactiontype, format->gettype ());
}

sttx::sttx (stobject const& object)
    : stobject (object)
    , sig_state_ (boost::indeterminate)
{
    tx_type_ = static_cast <txtype> (getfieldu16 (sftransactiontype));

    auto format = txformats::getinstance().findbytype (tx_type_);

    if (!format)
    {
        writelog (lswarning, sttx) <<
            "transaction type: " << tx_type_;
        throw std::runtime_error ("invalid transaction type");
    }

    if (!settype (format->elements))
    {
        writelog (lswarning, sttx) <<
            "transaction not legal for format";
        throw std::runtime_error ("transaction not valid");
    }
}

sttx::sttx (serializeriterator& sit)
    : stobject (sftransaction)
    , sig_state_ (boost::indeterminate)
{
    int length = sit.getbytesleft ();

    if ((length < protocol::txminsizebytes) || (length > protocol::txmaxsizebytes))
    {
        writelog (lserror, sttx) <<
            "transaction has invalid length: " << length;
        throw std::runtime_error ("transaction length invalid");
    }

    set (sit);
    tx_type_ = static_cast<txtype> (getfieldu16 (sftransactiontype));

    auto format = txformats::getinstance().findbytype (tx_type_);

    if (!format)
    {
        writelog (lswarning, sttx) <<
            "invalid transaction type: " << tx_type_;
        throw std::runtime_error ("invalid transaction type");
    }

    if (!settype (format->elements))
    {
        writelog (lswarning, sttx) <<
            "transaction not legal for format";
        throw std::runtime_error ("transaction not valid");
    }
}

std::string
sttx::getfulltext () const
{
    std::string ret = "\"";
    ret += to_string (gettransactionid ());
    ret += "\" = {";
    ret += stobject::getfulltext ();
    ret += "}";
    return ret;
}

std::vector<rippleaddress>
sttx::getmentionedaccounts () const
{
    std::vector<rippleaddress> accounts;

    for (auto const& it : peekdata ())
    {
        if (auto sa = dynamic_cast<staccount const*> (&it))
        {
            auto const na = sa->getvaluenca ();

            if (std::find (accounts.cbegin (), accounts.cend (), na) == accounts.cend ())
                accounts.push_back (na);
        }
        else if (auto sa = dynamic_cast<stamount const*> (&it))
        {
            auto const& issuer = sa->getissuer ();

            if (isnative (issuer))
                continue;

            rippleaddress na;
            na.setaccountid (issuer);

            if (std::find (accounts.cbegin (), accounts.cend (), na) == accounts.cend ())
                accounts.push_back (na);
        }
    }

    return accounts;
}

uint256
sttx::getsigninghash () const
{
    return stobject::getsigninghash (hashprefix::txsign);
}

uint256
sttx::gettransactionid () const
{
    return gethash (hashprefix::transactionid);
}

blob sttx::getsignature () const
{
    try
    {
        return getfieldvl (sftxnsignature);
    }
    catch (...)
    {
        return blob ();
    }
}

void sttx::sign (rippleaddress const& private_key)
{
    blob signature;
    private_key.accountprivatesign (getsigninghash (), signature);
    setfieldvl (sftxnsignature, signature);
}

bool sttx::checksign () const
{
    if (boost::indeterminate (sig_state_))
    {
        try
        {
            ecdsa const fullycanonical = (getflags() & tffullycanonicalsig)
                ? ecdsa::strict
                : ecdsa::not_strict;

            rippleaddress n;
            n.setaccountpublic (getfieldvl (sfsigningpubkey));

            sig_state_ = n.accountpublicverify (getsigninghash (),
                getfieldvl (sftxnsignature), fullycanonical);
        }
        catch (...)
        {
            sig_state_ = false;
        }
    }

    assert (!boost::indeterminate (sig_state_));

    return static_cast<bool> (sig_state_);
}

void sttx::setsigningpubkey (rippleaddress const& nasignpubkey)
{
    setfieldvl (sfsigningpubkey, nasignpubkey.getaccountpublic ());
}

void sttx::setsourceaccount (rippleaddress const& nasource)
{
    setfieldaccount (sfaccount, nasource);
}

json::value sttx::getjson (int) const
{
    json::value ret = stobject::getjson (0);
    ret["hash"] = to_string (gettransactionid ());
    return ret;
}

json::value sttx::getjson (int options, bool binary) const
{
    if (binary)
    {
        json::value ret;
        serializer s = stobject::getserializer ();
        ret["tx"] = strhex (s.peekdata ());
        ret["hash"] = to_string (gettransactionid ());
        return ret;
    }
    return getjson(options);
}

std::string const&
sttx::getmetasqlinsertreplaceheader (const database::type dbtype)
{
    if (dbtype == database::type::mysql)
    {
        static std::string const sqlmysql = "replace into transactions "
        "(transid, transtype, fromacct, fromseq, ledgerseq, status, closetime, rawtxn, txnmeta)"
        " values ";
        return sqlmysql;
    }
    
    static std::string const sql = "insert or replace into transactions "
        "(transid, transtype, fromacct, fromseq, ledgerseq, status, closetime, rawtxn, txnmeta)"
        " values ";

    return sql;
}

std::string sttx::getmetasql (std::uint32_t inledger,
    std::string const& escapedmetadata,
    std::uint32_t closetime) const
{
    serializer s;
    add (s);
    return getmetasql (s, inledger, txn_sql_validated, escapedmetadata, closetime);
}

std::string
sttx::getmetasql (serializer rawtxn,
    std::uint32_t inledger, char status, std::string const& escapedmetadata, std::uint32_t closetime) const
{
    static boost::format bftrans ("('%s', '%s', '%s', '%d', '%d', '%c', '%d', %s, %s)");
    std::string rtxn = sqlescape (rawtxn.peekdata ());

    auto format = txformats::getinstance().findbytype (tx_type_);
    assert (format != nullptr);

    return str (boost::format (bftrans)
                % to_string (gettransactionid ()) % format->getname ()
                % getsourceaccount ().humanaccountid ()
                % getsequence () % inledger % status % closetime % rtxn % escapedmetadata);
}

//------------------------------------------------------------------------------

static
bool
ismemookay (stobject const& st, std::string& reason)
{
    if (!st.isfieldpresent (sfmemos))
        return true;

    const starray& memos = st.getfieldarray (sfmemos);

    // the number 2048 is a preallocation hint, not a hard limit
    // to avoid allocate/copy/free's
    serializer s (2048);
    memos.add (s);

    // fixme move the memo limit into a config tunable
    if (s.getdatalength () > 1024)
    {
        reason = "the memo exceeds the maximum allowed size.";
        return false;
    }

    for (auto const& memo : memos)
    {
        auto memoobj = dynamic_cast <stobject const*> (&memo);

        if (!memoobj || (memoobj->getfname() != sfmemo))
        {
            reason = "a memo array may contain only memo objects.";
            return false;
        }

        for (auto const& memoelement : *memoobj)
        {
            if ((memoelement.getfname() != sfmemotype) &&
                (memoelement.getfname() != sfmemodata) &&
                (memoelement.getfname() != sfmemoformat))
            {
                reason = "a memo may contain only memotype, memodata or memoformat fields.";
                return false;
            }
        }
    }

    return true;
}

// ensure all account fields are 160-bits
static
bool
isaccountfieldokay (stobject const& st)
{
    for (int i = 0; i < st.getcount(); ++i)
    {
        auto t = dynamic_cast<staccount const*>(st.peekatpindex (i));
        if (t && !t->isvalueh160 ())
            return false;
    }

    return true;
}

bool passeslocalchecks (stobject const& st, std::string& reason)
{
    if (!ismemookay (st, reason))
        return false;

    if (!isaccountfieldokay (st))
    {
        reason = "an account field is invalid.";
        return false;
    }

    return true;
}

bool passeslocalchecks (stobject const& st)
{
    std::string reason;
    return passeslocalchecks (st, reason);
}

} // ripple
