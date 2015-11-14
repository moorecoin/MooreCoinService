#include <beastconfig.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/txflags.h>

namespace ripple {

class addreferee
    : public transactor
{
public:
    addreferee(
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("addreferee"))
    {

    }

    ter doapply () override
    {
        account const refereeid (mtxn.getfieldaccount160 (sfdestination));
        account const referenceid (mtxnaccountid);

        if (!refereeid)
        {
            m_journal.warning <<
                "malformed transaction: referee account not specified.";

            return temdst_needed;
        }
        else if (referenceid == refereeid)
        {
            // you're referring yourself.
            m_journal.trace <<
                "malformed transaction: redundant transaction:" <<
                " reference=" << to_string(referenceid) <<
                " referee=" << to_string(refereeid);

            return teminvalid;
        }

        return mengine->view ().addrefer(refereeid, referenceid);
    }
};

ter
transact_addreferee (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    return addreferee(txn, params, engine).apply();
}

}  // ripple
