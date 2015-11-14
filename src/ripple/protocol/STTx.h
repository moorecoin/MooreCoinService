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

#ifndef ripple_serializedtransaction_h
#define ripple_serializedtransaction_h

#include <ripple/app/data/database.h>
#include <ripple/protocol/stobject.h>
#include <ripple/protocol/txformats.h>
#include <boost/logic/tribool.hpp>

namespace ripple {

// vfalco todo replace these macros with language constants
#define txn_sql_new         'n'
#define txn_sql_conflict    'c'
#define txn_sql_held        'h'
#define txn_sql_validated   'v'
#define txn_sql_included    'i'
#define txn_sql_unknown     'u'

class sttx
    : public stobject
    , public countedobject <sttx>
{
public:
    static char const* getcountedobjectname () { return "sttx"; }

    typedef std::shared_ptr<sttx>        pointer;
    typedef const std::shared_ptr<sttx>& ref;

public:
    sttx () = delete;
    sttx& operator= (sttx const& other) = delete;

    sttx (sttx const& other) = default;

    explicit sttx (serializeriterator& sit);
    explicit sttx (txtype type);
    
    // only called from ripple::rpc::transactionsign - can we eliminate this?
    explicit sttx (stobject const& object);

    // stobject functions
    serializedtypeid getstype () const
    {
        return sti_transaction;
    }
    std::string getfulltext () const;

    // outer transaction functions / signature functions
    blob getsignature () const;

    uint256 getsigninghash () const;

    txtype gettxntype () const
    {
        return tx_type_;
    }
    stamount gettransactionfee () const
    {
        return getfieldamount (sffee);
    }
    void settransactionfee (const stamount & fee)
    {
        setfieldamount (sffee, fee);
    }

    rippleaddress getsourceaccount () const
    {
        return getfieldaccount (sfaccount);
    }
    blob getsigningpubkey () const
    {
        return getfieldvl (sfsigningpubkey);
    }
    void setsigningpubkey (const rippleaddress & nasignpubkey);
    void setsourceaccount (const rippleaddress & nasource);

    std::uint32_t getsequence () const
    {
        return getfieldu32 (sfsequence);
    }
    void setsequence (std::uint32_t seq)
    {
        return setfieldu32 (sfsequence, seq);
    }

    std::vector<rippleaddress> getmentionedaccounts () const;

    uint256 gettransactionid () const;

    virtual json::value getjson (int options) const;
    virtual json::value getjson (int options, bool binary) const;

    void sign (rippleaddress const& private_key);

    bool checksign () const;

    bool isknowngood () const
    {
        return (sig_state_ == true);
    }
    bool isknownbad () const
    {
        return (sig_state_ == false);
    }
    void setgood () const
    {
        sig_state_ = true;
    }
    void setbad () const
    {
        sig_state_ = false;
    }

    // sql functions with metadata
    static
    std::string const&
    getmetasqlinsertreplaceheader (const database::type dbtype);

    std::string getmetasql (
        std::uint32_t inledger, std::string const& escapedmetadata, std::uint32_t closetime) const;

    std::string getmetasql (
        serializer rawtxn,
        std::uint32_t inledger,
        char status,
        std::string const& escapedmetadata,
        std::uint32_t closetime) const;

private:
    sttx* duplicate () const override
    {
        return new sttx (*this);
    }

    txtype tx_type_;

    mutable boost::tribool sig_state_;
};

bool passeslocalchecks (stobject const& st, std::string&);
bool passeslocalchecks (stobject const& st);

} // ripple

#endif
