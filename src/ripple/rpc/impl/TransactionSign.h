//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012-2014 ripple labs inc.

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

#ifndef ripple_rpc_transactionsign_h_included
#define ripple_rpc_transactionsign_h_included

namespace ripple {
namespace rpc {

namespace rpcdetail {
// a class that allows these methods to be called with or without a
// real networkops instance.  this allows for unit testing.
class ledgerfacade
{
private:
    networkops* const netops_;
    ledger::pointer ledger_;
    rippleaddress accountid_;
    accountstate::pointer accountstate_;

public:
    // enum used to construct a facade for unit tests.
    enum nonetworkops{
        nonetops
    };

    ledgerfacade () = delete;
    ledgerfacade (ledgerfacade const&) = delete;
    ledgerfacade& operator= (ledgerfacade const&) = delete;

    // for use in non unit testing circumstances.
    explicit ledgerfacade (networkops& netops)
    : netops_ (&netops)
    { }

    // for testtransactionrpc unit tests.
    explicit ledgerfacade (nonetworkops noops)
    : netops_ (nullptr) { }

    // for testautofillfees unit tests.
    ledgerfacade (nonetworkops noops, ledger::pointer ledger)
    : netops_ (nullptr)
    , ledger_ (ledger)
    { }

    void snapshotaccountstate (rippleaddress const& accountid);

    bool isvalidaccount () const;

    std::uint32_t getseq () const;

    bool findpathsforoneissuer (
        rippleaddress const& dstaccountid,
        issue const& srcissue,
        stamount const& dstamount,
        int searchlevel,
        unsigned int const maxpaths,
        stpathset& pathsout,
        stpath& fullliquiditypath) const;

    transaction::pointer submittransactionsync (
        transaction::ref tptrans,
        bool badmin,
        bool blocal,
        bool bfailhard,
        bool bsubmit);

    std::uint64_t scalefeebase (std::uint64_t fee) const;

    std::uint64_t scalefeeload (std::uint64_t fee, bool badmin) const;

    bool hasaccountroot () const;
    bool isaccountexist (const account& account) const;

    bool accountmasterdisabled () const;

    bool accountmatchesregularkey (account account) const;

    int getvalidatedledgerage () const;

    bool isloadedcluster () const;
};

} // namespace rpcdetail


/** returns a json::objectvalue. */
json::value transactionsign (
    json::value params,
    bool bsubmit,
    bool bfailhard,
    rpcdetail::ledgerfacade& ledgerfacade,
    role role);

inline json::value transactionsign (
    json::value params,
    bool bsubmit,
    bool bfailhard,
    networkops& netops,
    role role)
{
    rpcdetail::ledgerfacade ledgerfacade (netops);
    return transactionsign (params, bsubmit, bfailhard, ledgerfacade, role);
}

} // rpc
} // ripple

#endif
