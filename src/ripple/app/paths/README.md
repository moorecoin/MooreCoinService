theory of pathfinding
=====================

it's a hard problem - an exponential problem - and there is a limited amount of time until the ledger changes.

search for the best path, but it's impossible to guarantee the result is the best unless the ledger is small.

also, the current state of the ledger can't be definitively known. we only know the state of past ledgers.

by the time a transaction is processed, liquidity may be taken or added. this is why payments support a maximum cost.

people are given estimates so they can make a decision as to whether the overall cost is good enough for them.

this is the **rippled** implementation; there are many other possible implementations!

**rippled** uses a variety of search techniques:

1. hebbian learning.
 * reuse found liquidity.
 * good liquidity is often reusable.
 * when searching, limit our search to the rippled cache.
2. six degrees of separation.
 * if sending value through individual's account, expect no path to have more than six hops.
 * according to [facebook studies](https://www.facebook.com/notes/facebook-data-team/anatomy-of-facebook/10150388519243859) as of late 2011, it's users are separated by fewer than five steps.
 * by using xrp for bridging the most complicated path expected is usually:
 * source -> gateway -> xrp -> gateway -> destination
3. pareto principle.
 * good liquidity is often reusable.
 * concentrate on the most liquid gateways to serve almost everybody.
 * people who chose to use illiquid gateways get the service level implied by their choice of poor liquidity.
4. monte carlo methods.
 * learn new paths.
 * search outside the **rippled** cache.
 * distributed learning.
 * every rippled node searches their own cache for best liquidity and then the good results get propagated to the network.
 * whenever somebody makes a payment, the nodes involved go to the rippled cache; since payments appear in every ledger in the network, this liquidity information now appears in every rippled cache.


life of a payment
======================

overview
----------

making a payment in the ripple network is about finding the cheapest path between source and destination.

there are various stages:

an issue is a balance in some specific currency.  an issuer is someone who "creates" a currency by creating an issue.

for tx processing, people submit a tx to a rippled node, which attempts to apply the tx locally first, and if succeesful, distributes it to other nodes.

when someone accepts payment they list their specific payment terms, "what must happen before the payment goes off."  that can be done completely in the ripple network.  normally a payment on the ripple net can be completely settled there.  when the ledger closes, the terms are met, or they are not met.

for a bridge payment, based on a previous promise it will deliver payment off-network. a bridge promises to execute the final leg of a payment, but there's the possibility of default. if you want to trust a bridge for a specific pathfinding request, you need to include it in the request.

in contrast, a gateway is in the business of redeeming value on the ripple network for value off the ripnet.  a gateway is in the business of issuing balances.

bitstamp is a gateway - you send 'em cash, they give you a ripple balance and vice versa.  there's no promise or forwarding for a transaction.

a bridge is a facility that allows payments to be made from the ripnet to off the ripnet.

suppose i'm on the ripple network and want to send value to the bitcoin network.  see:  https://ripple.com/wiki/outbound_bridge_payments
https://ripple.com/wiki/services_api


two types of paths:
1. i have x amount of cash i wish to send to someone - how much will they receive?
 * not yet implemented in pathfinding, or at least in the rpc api.
   * todo: find if this is implemented and either document it or file a jira.
2. i need to deliver x amount of cash to someone - how much will it cost me?

here's a transaction:
https://ripple.com/wiki/transaction_format#payment_.280.29

not implemented: bridge types.



high level of a payment
-----------------------

1. i make a request for path finding to a rippled.
2. that rippled "continuously" returns solutions until i "stop".
3. you create a transaction which includes the path set that you got from the quote.
4. you sign it.
5. you submit it to one or more rippleds.
6. those rippled validates it, and forwards it if it;s valid.
 * valid means "not malformed" and "can claim a fee" - the sending account has enough to cover the transaction fee.
7. at ledger closing time, the transaction is applied by the transaction engine and the result is stored by changing the ledger and storing the tx metadata in that ledger.

if you're sending and receiving xrp you don't need a path.
if the union of the default paths are sufficient, you don't need a path set (see below about default paths).
the idea behind path sets is to provide sufficient liquidity in the face of a changing ledger.

if you can compute a path already yourself or know one, you don't need to do steps 1 or 2.


1. finding liquidity - "path finding"
  * finding paths (exactly "path finding")
  * filtering by liquidity.
    * take a selection of paths that should satisfy the transaction's conditions.
    * this routine is called ripplecalc::ripplecalc.

2. build a payment transaction containing the path set.
  * a path set is a group of paths that a transaction is supposed to use for liquidity
  * default paths are not included
    * zero hop path: a default path is "source directly to destination".
    * one hop paths - because they can be derived from the source and destination currency.
  * a payment transaction includes the following fields
    * the source account
    * the destination account
    * destination amount
      * contains an amount and a currency
      * if currency not xrp, must have a specified issuer.
      * if you specify an issuer then you must deliver that issuance to the destination.
      * if you specify the issuer as the destination account, then they will receive any of the issuances they trust.
    * maximum source amount - maximum amount to debit the sender.
      * this field is optional.
      * if not specified, defaults to the destination amount with the sender as issuer.
      * contains an amount and a currency
      * if currency not xrp, must have a specified issuer.
      * specifying the sender as issuer allows any of the sender's issuance to be spent.
      * specifying a specific issuer allows only that specific issuance to be sent.
  * path set.
    * optional.
    * might contain "invalid" paths or even "gibberish" for an untrusted server.
    * an untrusted server could provide proof that their paths are actually valid.
      * that would not prove that this is the cheapest path.
    * the client needs to talk to multiple untrusted servers, get proofs and then pick the best path.
    * it's much easier to validate a proof of a path than to find one, because you need a lot of information to find one.
       * in the future we might allow one server to validate a path offered by another server.

3. executing a payment
 * very little time, can't afford to do a search.
 * that's why we do the path building before the payment is due.
 * the routine used to compute liquidity and ledger change is also called ripplecalc::ripplecalc.





