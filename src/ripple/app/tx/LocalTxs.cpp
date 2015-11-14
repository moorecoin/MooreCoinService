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
#include <ripple/app/tx/localtxs.h>
#include <ripple/app/misc/canonicaltxset.h>

/*
 this code prevents scenarios like the following:
1) a client submits a transaction.
2) the transaction gets into the ledger this server
   believes will be the consensus ledger.
3) the server builds a succeeding open ledger without the
   transaction (because it's in the prior ledger).
4) the local consensus ledger is not the majority ledger
   (due to network conditions, byzantine fault, etcetera)
   the majority ledger does not include the transaction.
5) the server builds a new open ledger that does not include
   the transaction or have it in a prior ledger.
6) the client submits another transaction and gets a terpre_seq
   preliminary result.
7) the server does not relay that second transaction, at least
   not yet.

with this code, when step 5 happens, the first transaction will
be applied to that open ledger so the second transaction will
succeed normally at step 6. transactions remain tracked and
test-applied to all new open ledgers until seen in a fully-
validated ledger
*/

namespace ripple {

// this class wraps a pointer to a transaction along with
// its expiration ledger. it also caches the issuing account.
class localtx
{
public:

    // the number of ledgers to hold a transaction is essentially
    // arbitrary. it should be sufficient to allow the transaction to
    // get into a fully-validated ledger.
    static int const holdledgers = 5;

    localtx (ledgerindex index, sttx::ref txn)
        : m_txn (txn)
        , m_expire (index + holdledgers)
        , m_id (txn->gettransactionid ())
        , m_account (txn->getsourceaccount ())
        , m_seq (txn->getsequence())
    {
        if (txn->isfieldpresent (sflastledgersequence))
            m_expire = std::min (m_expire, txn->getfieldu32 (sflastledgersequence) + 1);
    }

    uint256 const& getid () const
    {
        return m_id;
    }

    std::uint32_t getseq () const
    {
        return m_seq;
    }

    bool isexpired (ledgerindex i) const
    {
        return i > m_expire;
    }

    sttx::ref gettx () const
    {
        return m_txn;
    }

    rippleaddress const& getaccount () const
    {
        return m_account;
    }

private:

    sttx::pointer m_txn;
    ledgerindex                    m_expire;
    uint256                        m_id;
    rippleaddress                  m_account;
    std::uint32_t                  m_seq;
};

class localtxsimp : public localtxs
{
public:

    localtxsimp()
    { }

    // add a new transaction to the set of local transactions
    void push_back (ledgerindex index, sttx::ref txn) override
    {
        std::lock_guard <std::mutex> lock (m_lock);

        m_txns.emplace_back (index, txn);
    }

    bool can_remove (localtx& txn, ledger::ref ledger)
    {

        if (txn.isexpired (ledger->getledgerseq ()))
            return true;

        if (ledger->hastransaction (txn.getid ()))
            return true;

        sle::pointer sle = ledger->getaccountroot (txn.getaccount ());
        if (!sle)
            return false;

        if (sle->getfieldu32 (sfsequence) > txn.getseq ())
            return true;


        return false;
    }

    void apply (transactionengine& engine) override
    {

        canonicaltxset tset (uint256 {});

        // get the set of local transactions as a canonical
        // set (so they apply in a valid order)
        {
            std::lock_guard <std::mutex> lock (m_lock);

            for (auto& it : m_txns)
                tset.push_back (it.gettx());
        }

        for (auto it : tset)
        {
            try
            {
                transactionengineparams parms = tapopen_ledger;
                bool didapply;
                engine.applytransaction (*it.second, parms, didapply);
            }
            catch (...)
            {
                // nothing special we need to do.
                // it's possible a cleverly malformed transaction or
                // corrupt back end database could cause an exception
                // during transaction processing.
            }
        }
    }

    // remove transactions that have either been accepted into a fully-validated
    // ledger, are (now) impossible, or have expired
    void sweep (ledger::ref validledger) override
    {
        std::lock_guard <std::mutex> lock (m_lock);

        for (auto it = m_txns.begin (); it != m_txns.end (); )
        {
            if (can_remove (*it, validledger))
                it = m_txns.erase (it);
            else
                ++it;
        }
    }

    std::size_t size () override
    {
        std::lock_guard <std::mutex> lock (m_lock);

        return m_txns.size ();
    }

private:

    std::mutex m_lock;
    std::list <localtx> m_txns;
};

std::unique_ptr <localtxs> localtxs::new()
{
    return std::make_unique <localtxsimp> ();
}

} // ripple
