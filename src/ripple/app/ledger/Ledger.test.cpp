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

#include <ripple/app/ledger/ledger.h>
#include <ripple/app/consensus/ledgerconsensus.h>
#include <ripple/app/ledger/ledgertiming.h>
#include <ripple/app/misc/canonicaltxset.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/seconds_clock.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/rippleaddress.h>
#include <ripple/protocol/stparsedjson.h>
#include <ripple/protocol/txflags.h>
#include <beast/unit_test/suite.h>

namespace ripple {

class ledger_test : public beast::unit_test::suite
{
    using testaccount = std::pair<rippleaddress, unsigned>;

    struct amount
    {
        amount (double value_, std::string currency_, testaccount issuer_)
            : value(value_)
            , currency(currency_)
            , issuer(issuer_)
        {
        }

        double value;
        std::string currency;
        testaccount issuer;

        json::value
        getjson() const
        {
            json::value tx_json;
            tx_json["currency"] = currency;
            tx_json["issuer"] = issuer.first.humanaccountid();
            tx_json["value"] = std::to_string(value);
            return tx_json;
        }
    };

    // helper function to parse a transaction in json, sign it with account,
    // and return it as a sttx
    sttx
    parsetransaction(testaccount& account, json::value const& tx_json)
    {
        stparsedjsonobject parsed("tx_json", tx_json);
        std::unique_ptr<stobject> soptrans = std::move(parsed.object);
        expect(soptrans != nullptr);
        soptrans->setfieldvl(sfsigningpubkey, account.first.getaccountpublic());
        return sttx(*soptrans);
    }

    // helper function to apply a transaction to a ledger
    void
    applytransaction(ledger::pointer const& ledger, sttx const& tx)
    {
        transactionengine engine(ledger);
        bool didapply = false;
        auto r = engine.applytransaction(tx, tapopen_ledger | tapno_check_sign,
                                         didapply);
        expect(r == tessuccess);
        expect(didapply);
    }

    // create genesis ledger from a start amount in drops, and the public
    // master rippleaddress
    ledger::pointer
    creategenesisledger(std::uint64_t start_amount_drops, testaccount const& master)
    {
        ledger::pointer ledger = std::make_shared<ledger>(master.first,
                                                          start_amount_drops,
                                                          start_amount_drops);
        ledger->updatehash();
        ledger->setclosed();
        expect(ledger->assertsane());
        return ledger;
    }

    // create an account represented by public rippleaddress and private
    // rippleaddress
    testaccount
    createaccount()
    {
        static rippleaddress const seed
                = rippleaddress::createseedgeneric ("masterpassphrase");
        static rippleaddress const generator
                = rippleaddress::creategeneratorpublic (seed);
        static int iseq = -1;
        ++iseq;
        return std::make_pair(rippleaddress::createaccountpublic(generator, iseq),
                              std::uint64_t(0));
    }

    void
    freezeaccount(testaccount& account, ledger::pointer const& ledger)
    {
        json::value tx_json;
        tx_json["transactiontype"] = "accountset";
        tx_json["fee"] = std::to_string(1000);
        tx_json["account"] = account.first.humanaccountid();
        tx_json["setflag"] = asfglobalfreeze;
        tx_json["sequence"] = ++account.second;
        sttx tx = parsetransaction(account, tx_json);
        applytransaction(ledger, tx);
    }

    void
    unfreezeaccount(testaccount& account, ledger::pointer const& ledger)
    {
        json::value tx_json;
        tx_json["transactiontype"] = "accountset";
        tx_json["fee"] = std::to_string(1000);
        tx_json["account"] = account.first.humanaccountid();
        tx_json["clearflag"] = asfglobalfreeze;
        tx_json["sequence"] = ++account.second;
        sttx tx = parsetransaction(account, tx_json);
        applytransaction(ledger, tx);
    }

    void
    makepayment(testaccount& from, testaccount const& to,
                std::uint64_t amountdrops,
                std::uint64_t feedrops,
                ledger::pointer const& ledger)
    {
        json::value tx_json;
        tx_json["account"] = from.first.humanaccountid();
        tx_json["amount"] = std::to_string(amountdrops);
        tx_json["destination"] = to.first.humanaccountid();
        tx_json["transactiontype"] = "payment";
        tx_json["fee"] = std::to_string(feedrops);
        tx_json["sequence"] = ++from.second;
        tx_json["flags"] = tfuniversal;
        sttx tx = parsetransaction(from, tx_json);
        applytransaction(ledger, tx);
    }

    void
    makepaymentvbc(testaccount& from, testaccount const& to,
                   std::uint64_t amountdrops,
                   std::uint64_t feedrops,
                   ledger::pointer const& ledger)
    {
        json::value tx_json;
        tx_json["account"] = from.first.humanaccountid();
        json::value & amount = tx_json["amount"];
        amount["value"] = std::to_string(amountdrops);
        amount["currency"] = "vbc";
        tx_json["destination"] = to.first.humanaccountid();
        tx_json["transactiontype"] = "payment";
        tx_json["fee"] = std::to_string(feedrops);
        tx_json["sequence"] = ++from.second;
        tx_json["flags"] = tfuniversal;
        sttx tx = parsetransaction(from, tx_json);
        applytransaction(ledger, tx);
    }

    void
    makepayment(testaccount& from, testaccount const& to,
                amount const amount,
                ledger::pointer const& ledger)
    {
        json::value tx_json;
        tx_json["account"] = from.first.humanaccountid();
        tx_json["amount"] = amount.getjson();
        tx_json["destination"] = to.first.humanaccountid();
        tx_json["transactiontype"] = "payment";
        tx_json["fee"] = std::to_string(1000);
        tx_json["sequence"] = ++from.second;
        tx_json["flags"] = tfuniversal;
        sttx tx = parsetransaction(from, tx_json);
        applytransaction(ledger, tx);
    }

    void
    makepayment(testaccount& from, testaccount const& to,
                std::string const& currency, std::string const& amount,
                ledger::pointer const& ledger)
    {
        makepayment(from, to, amount(std::stod(amount), currency, to), ledger);
    }

    void
    makeissue(testaccount& from, testaccount const& to,
              std::string const& amount,
              ledger::pointer const& ledger)
    {
        json::value tx_json;
        tx_json["account"] = from.first.humanaccountid();
        tx_json["amount"] = amount(std::stod(amount), to_string(assetcurrency()), from).getjson();
        tx_json["destination"] = to.first.humanaccountid();
        json::value& releaseschedule = tx_json["releaseschedule"];
        json::value releasepoint;
        json::value& releaserate = releasepoint["releasepoint"];
        releaserate["expiration"] = 0;
        releaserate["releaserate"] = 100000000;
        releaseschedule.append(releasepoint);
        releaserate["expiration"] = 86400;
        releaserate["releaserate"] = 900000000;
        releaseschedule.append(releasepoint);
        tx_json["transactiontype"] = "issue";
        tx_json["fee"] = std::to_string(1000);
        tx_json["sequence"] = ++from.second;
        tx_json["flags"] = tfuniversal;
        sttx tx = parsetransaction(from, tx_json);
        applytransaction(ledger, tx);
    }

    void
    createoffer(testaccount& from, amount const& in, amount const& out,
                ledger::pointer ledger)
    {
        json::value tx_json;
        tx_json["transactiontype"] = "offercreate";
        tx_json["fee"] = std::to_string(1000);
        tx_json["account"] = from.first.humanaccountid();
        tx_json["takerpays"] = in.getjson();
        tx_json["takergets"] = out.getjson();
        tx_json["sequence"] = ++from.second;
        sttx tx = parsetransaction(from, tx_json);
        applytransaction(ledger, tx);
    }

    // as currently implemented, this will cancel only the last offer made
    // from this account.
    void
    canceloffer(testaccount& from, ledger::pointer ledger)
    {
        json::value tx_json;
        tx_json["transactiontype"] = "offercancel";
        tx_json["fee"] = std::to_string(1000);
        tx_json["account"] = from.first.humanaccountid();
        tx_json["offersequence"] = from.second;
        tx_json["sequence"] = ++from.second;
        sttx tx = parsetransaction(from, tx_json);
        applytransaction(ledger, tx);
    }

    void
    maketrustset(testaccount& from, testaccount const& issuer,
                 std::string const& currency, double amount,
                 ledger::pointer const& ledger, uint32_t flags = tfclearnoripple)
    {
        json::value tx_json;
        tx_json["account"] = from.first.humanaccountid();
        json::value& limitamount = tx_json["limitamount"];
        limitamount["currency"] = currency;
        limitamount["issuer"] = issuer.first.humanaccountid();
        limitamount["value"] = std::to_string(amount);
        tx_json["transactiontype"] = "trustset";
        tx_json["fee"] = std::to_string(1000);
        tx_json["sequence"] = ++from.second;
        tx_json["flags"] = flags;
        sttx tx = parsetransaction(from, tx_json);
        applytransaction(ledger, tx);
    }

    ledger::pointer
    close_and_advance(ledger::pointer ledger, ledger::pointer lcl)
    {
        shamap::pointer set = ledger->peektransactionmap();
        canonicaltxset retriabletransactions(set->gethash());
        ledger::pointer newlcl = std::make_shared<ledger>(false, *lcl);
        // set up to write shamap changes to our database,
        //   perform updates, extract changes
        applytransactions(set, newlcl, newlcl, retriabletransactions, false);
        newlcl->updateskiplist();
        newlcl->setclosed();
        newlcl->peekaccountstatemap()->flushdirty(
            hotaccount_node, newlcl->getledgerseq());
        newlcl->peektransactionmap()->flushdirty(
            hottransaction_node, newlcl->getledgerseq());
        using namespace std::chrono;
        auto const epoch_offset = days(10957);  // 2000-01-01
        std::uint32_t closetime = time_point_cast<seconds>  // now
                                         (system_clock::now()-epoch_offset).
                                         time_since_epoch().count();
        int closeresolution = seconds(ledger_time_accuracy).count();
        bool closetimecorrect = true;
        newlcl->setaccepted(closetime, closeresolution, closetimecorrect);
        return newlcl;
    }

    void test_genesisledger ()
    {
        std::uint64_t const xrp = std::mega::num;

        // create master account
        auto master = createaccount();

        // create genesis ledger
        ledger::pointer lcl = creategenesisledger(100000*xrp, master);

        // create open scratch ledger
        ledger::pointer ledger = std::make_shared<ledger>(false, *lcl);

        // create user accounts
        auto gw1 = createaccount();
        auto gw2 = createaccount();
        auto gw3 = createaccount();
        auto alice = createaccount();
        auto mark = createaccount();

        // fund gw1, gw2, gw3, alice, mark from master
        makepayment(master, gw1, 5000*xrp, (0.01+50)*xrp, ledger);
        makepayment(master, gw2, 4000*xrp, (0.01+40)*xrp, ledger);
        makepayment(master, gw3, 3000*xrp, (0.01+30)*xrp, ledger);
        makepayment(master, alice, 2000*xrp, (0.01+20)*xrp, ledger);
        
        makepaymentvbc(master, gw1, 5000*xrp, (50)*xrp, ledger);
        makepaymentvbc(master, gw2, 4000*xrp, (40)*xrp, ledger);
        makepaymentvbc(master, gw3, 3000*xrp, (30)*xrp, ledger);
        makepaymentvbc(master, alice, 2000*xrp, (20)*xrp, ledger);
        
        makepaymentvbc(master, mark, 1000*xrp, (0.01+10)*xrp, ledger);
        makepayment(master, mark, 1000*xrp, (10)*xrp, ledger);

        lcl = close_and_advance(ledger, lcl);
        ledger = std::make_shared<ledger>(false, *lcl);

        // alice trusts foo/gw1
        maketrustset(alice, gw1, "foo", 1, ledger);

        // mark trusts foo/gw2
        maketrustset(mark, gw2, "foo", 1, ledger);

        // mark trusts foo/gw3
        maketrustset(mark, gw3, "foo", 1, ledger);

        // gw2 pays mark with foo
        makepayment(gw2, mark, "foo", ".1", ledger);

        // gw3 pays mark with foo
        makepayment(gw3, mark, "foo", ".2", ledger);

        // gw1 pays alice with foo
        makepayment(gw1, alice, "foo", ".3", ledger);

        lcl = close_and_advance(ledger, lcl);
        ledger = std::make_shared<ledger>(false, *lcl);

        createoffer(mark, amount(1, "foo", gw1), amount(1, "foo", gw2), ledger);
        createoffer(mark, amount(1, "foo", gw2), amount(1, "foo", gw3), ledger);
        canceloffer(mark, ledger);
        freezeaccount(alice, ledger);

        lcl = close_and_advance(ledger, lcl);
        ledger = std::make_shared<ledger>(false, *lcl);

        makepayment(alice, mark, 1*xrp, (0.001)*xrp, ledger);

        lcl = close_and_advance(ledger, lcl);
        ledger = std::make_shared<ledger>(false, *lcl);

        // gw1 issue asset
        makeissue(gw1, mark, "1000", ledger);

        lcl = close_and_advance(ledger, lcl);
        ledger = std::make_shared<ledger>(false, *lcl);

        // mark trusts asset/gw1
        maketrustset(gw2, gw1, to_string(assetcurrency()), 10, ledger, tfsetnoripple);

        lcl = close_and_advance(ledger, lcl);
        ledger = std::make_shared<ledger>(false, *lcl);

        // gw1 pays alice with foo
        makepayment(mark, gw2, amount(5, to_string(assetcurrency()), gw1), ledger);

        lcl = close_and_advance(ledger, lcl);
        ledger = std::make_shared<ledger>(false, *lcl);
    }

    void test_getquality ()
    {
        uint256 ubig (
            "d2dc44e5dc189318db36ef87d2104cdf0a0fe3a4b698beee55038d7ea4c68000");
        expect (6125895493223874560 == getquality (ubig));
    }
public:
    void run ()
    {
        test_genesisledger ();
        test_getquality ();
    }
};

beast_define_testsuite(ledger,ripple_app,ripple);

} // ripple
