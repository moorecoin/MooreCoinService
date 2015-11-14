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

#ifndef ripple_ledgerholder_h
#define ripple_ledgerholder_h

namespace ripple {

// can std::atomic<std::shared_ptr>> make this lock free?

/** hold a ledger in a thread-safe way.
*/
class ledgerholder
{
public:
    typedef ripplemutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;

    // update the held ledger
    void set (ledger::pointer ledger)
    {
        // the held ledger must always be immutable
        if (ledger && !ledger->isimmutable ())
           ledger = std::make_shared <ledger> (*ledger, false);

        {
            scopedlocktype sl (m_lock);

            m_heldledger = ledger;
        }
    }

    // return the (immutable) held ledger
    ledger::pointer get ()
    {
        scopedlocktype sl (m_lock);

        return m_heldledger;
    }

    // return a mutable snapshot of the held ledger
    ledger::pointer getmutable ()
    {
        ledger::pointer ret = get ();
        return ret ? std::make_shared <ledger> (*ret, true) : ret;
    }


    bool empty ()
    {
        scopedlocktype sl (m_lock);

        return m_heldledger == nullptr;
    }

private:

    locktype m_lock;
    ledger::pointer m_heldledger;

};

} // ripple

#endif
