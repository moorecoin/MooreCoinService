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
#include <ripple/app/data/dbinit.h>
#include <type_traits>

namespace ripple {

// transaction database holds transactions and public keys
const char* txndbinit[] =
{
    "pragma synchronous=normal;",
    "pragma journal_mode=wal;",
    "pragma journal_size_limit=1582080;",

#if (ulong_max > uint_max) && !defined (no_sqlite_mmap)
    "pragma mmap_size=17179869184;",
#endif

    "begin transaction;",

    "create table transactions (                \
        transid     character(64) primary key,  \
        transtype   character(24),              \
        fromacct    character(35),              \
        fromseq     bigint unsigned,            \
        ledgerseq   bigint unsigned,            \
        status      character(1),               \
        closetime   integer default 0,          \
        rawtxn      blob,                       \
        txnmeta     blob                        \
    );",
    "create index txlgrindex on                 \
        transactions(ledgerseq);",

    "create table accounttransactions (         \
        transid     character(64),              \
        account     character(64),              \
        ledgerseq   bigint unsigned,            \
        txnseq      integer                     \
    );",
    "create index accttxidindex on              \
        accounttransactions(transid);",
    "create index accttxindex on                \
        accounttransactions(account, ledgerseq, txnseq, transid);",
    "create index acctlgrindex on               \
        accounttransactions(ledgerseq, account, transid);",

    "end transaction;"
    
};

int txndbcount = std::extent<decltype(txndbinit)>::value;
    
// mysql init
const char* txndbinitmysql[] =
{
    "start transaction;",
    
    "create table if not exists transactions (       \
        transid     character(64) primary key,      \
        transtype   character(24),                  \
        fromacct    character(35),                  \
        fromseq     bigint unsigned,                \
        ledgerseq   bigint unsigned,                \
        status      character(1),                   \
        closetime   integer default 0,               \
        rawtxn      longblob,                       \
        txnmeta     longblob                       \
    );",
    "create index txlgrindex on                     \
        transactions(ledgerseq);",
    
    "create table if not exists accounttransactions (    \
        transid     character(64),                      \
        account     character(64),                      \
        ledgerseq   bigint unsigned,                    \
        txnseq      integer                             \
    );",
    "create index accttxidindex on              \
        accounttransactions(transid);",
    "create index accttxindex on                \
        accounttransactions(account, ledgerseq, txnseq, transid);",
    "create index acctlgrindex on               \
        accounttransactions(ledgerseq, account, transid);",
    
    "commit;"
};

int txndbcountmysql = std::extent<decltype(txndbinitmysql)>::value;

// ledger database holds ledgers and ledger confirmations
const char* ledgerdbinit[] =
{
    "pragma synchronous=normal;",
    "pragma journal_mode=wal;",
    "pragma journal_size_limit=1582080;",

    "begin transaction;",

    "create table ledgers (                         \
        ledgerhash      character(64) primary key,  \
        ledgerseq       bigint unsigned,            \
        prevhash        character(64),              \
        totalcoins      bigint unsigned,            \
		totalcoinsvbc   bigint unsigned,            \
        closingtime     bigint unsigned,            \
        prevclosingtime bigint unsigned,            \
        closetimeres    bigint unsigned,            \
        closeflags      bigint unsigned,            \
        dividendledger  bigint unsigned,            \
        accountsethash  character(64),              \
        transsethash    character(64)               \
    );",
    "create index seqledger on ledgers(ledgerseq);",

    "create table validations   (                   \
        ledgerhash  character(64),                  \
        nodepubkey  character(56),                  \
        signtime    bigint unsigned,                \
        rawdata     blob                            \
    );",
    "create index validationsbyhash on              \
        validations(ledgerhash);",
    "create index validationsbytime on              \
        validations(signtime);",

    "end transaction;"
};

int ledgerdbcount = std::extent<decltype(ledgerdbinit)>::value;

// rpc database holds persistent data for rpc clients.
const char* rpcdbinit[] =
{

    // local persistence of the rpc client
    "create table rpcdata (                         \
        key         text primary key,               \
        value       text                            \
    );",
};

int rpcdbcount = std::extent<decltype(rpcdbinit)>::value;

// nodeidentity database holds local accounts and trusted nodes
// vfalco note but its a table not a database, so...?
//
const char* walletdbinit[] =
{
    // node identity must be persisted for cas routing and responsibilities.
    "begin transaction;",

    "create table nodeidentity (                    \
        publickey       character(53),              \
        privatekey      character(52),              \
        dh512           text,                       \
        dh1024          text                        \
    );",

    // miscellaneous persistent information
    // integer: 1 : used to simplify sql.
    // scoreupdated: when scores was last updated.
    // fetchupdated: when last fetch succeeded.
    "create table misc (                            \
        magic           integer unique not null,    \
        scoreupdated    datetime,                   \
        fetchupdated    datetime                    \
    );",

    // scoring and other information for domains.
    //
    // domain:
    //  domain source for https.
    // publickey:
    //  set if ever succeeded.
    // xxx use null in place of ""
    // source:
    //  'm' = manually added.   : 1500
    //  'v' = validators.txt    : 1000
    //  'w' = web browsing.     :  200
    //  'r' = referral          :    0
    // next:
    //  time of next fetch attempt.
    // scan:
    //  time of last fetch attempt.
    // fetch:
    //  time of last successful fetch.
    // sha256:
    //  checksum of last fetch.
    // comment:
    //  user supplied comment.
    // table of domains user has asked to trust.
    "create table seeddomains (                     \
        domain          text primary key not null,  \
        publickey       character(53),              \
        source          character(1) not null,      \
        next            datetime,                   \
        scan            datetime,                   \
        fetch           datetime,                   \
        sha256          character[64],              \
        comment         text                        \
    );",

    // allow us to easily find the next seeddomain to fetch.
    "create index seeddomainnext on seeddomains (next);",

    // table of publickeys user has asked to trust.
    // fetches are made to the cas.  this gets the ripple.txt so even validators
    // without a web server can publish a ripple.txt.
    // source:
    //  'm' = manually added.   : 1500
    //  'v' = validators.txt    : 1000
    //  'w' = web browsing.     :  200
    //  'r' = referral          :    0
    // next:
    //  time of next fetch attempt.
    // scan:
    //  time of last fetch attempt.
    // fetch:
    //  time of last successful fetch.
    // sha256:
    //  checksum of last fetch.
    // comment:
    //  user supplied comment.
    "create table seednodes (                       \
        publickey       character(53) primary key not null, \
        source          character(1) not null,      \
        next            datetime,                   \
        scan            datetime,                   \
        fetch           datetime,                   \
        sha256          character[64],              \
        comment         text                        \
    );",

    // allow us to easily find the next seednode to fetch.
    "create index seednodenext on seednodes (next);",

    // nodes we trust to not grossly collude against us.  derived from
    // seeddomains, seednodes, and validatorreferrals.
    //
    // score:
    //  computed trust score.  higher is better.
    // seen:
    //  last validation received.
    "create table trustednodes (                            \
        publickey       character(53) primary key not null, \
        score           integer default 0 not null,         \
        seen            datetime,                           \
        comment         text                                \
    );",

    // list of referrals.
    // - there may be multiple sources for a validator. the last source is used.
    // validator:
    //  public key of referrer.
    // entry:
    //  entry index in [validators] table.
    // referral:
    //  this is the form provided by the ripple.txt:
    //  - public key for cas based referral.
    //  - domain for domain based referral.
    // xxx do garbage collection when validators have no references.
    "create table validatorreferrals (              \
        validator       character(53) not null,     \
        entry           integer not null,           \
        referral        text not null,              \
        primary key (validator,entry)               \
    );",

    // list of referrals from ripple.txt files.
    // validator:
    //  public key of referree.
    // entry:
    //  entry index in [validators] table.
    // ip:
    //  ip of referred.
    // port:
    //  -1 = default
    // xxx do garbage collection when ips have no references.
    "create table ipreferrals (                         \
        validator       character(53) not null,         \
        entry           integer not null,               \
        ip              text not null,                  \
        port            integer not null default -1,    \
        primary key (validator,entry)                   \
    );",

    "create table features (                            \
        hash            character(64) primary key,      \
        firstmajority   bigint unsigned,                \
        lastmajority    bigint unsigned                 \
    );",

    // this removes an old table and its index which are now redundant. this
    // code will eventually go away. it's only here to clean up the wallet.db
    "drop table if exists peerips;",
    "drop index if exists;",


    "end transaction;"
};

int walletdbcount = std::extent<decltype(walletdbinit)>::value;

// hash node database holds nodes indexed by hash
// vfalco todo remove this since it looks unused
/*

int hashnodedbcount = std::extent<decltype(hashnodedbinit)>::value;
*/

// net node database holds nodes seen on the network
// xxx not really used needs replacement.
/*
const char* netnodedbinit[] =
{
    "create table knownnodes (                      \
        hanko           character(35) primary key,  \
        lastseen        text,                       \
        havecontactinfo character(1),               \
        contactobject   blob                        \
    );"
};

int netnodedbcount = std::extent<decltype(netnodedbinit)>::value;
*/

// this appears to be unused
/*
const char* pathfinddbinit[] =
{
    "pragma synchronous = off;            ",

    "drop table trustlines;               ",

    "create table trustlines {            "
    "to             character(40),    "  // hex of account trusted
    "by             character(40),    " // hex of account trusting
    "currency       character(80),    " // hex currency, hex issuer
    "use            integer,          " // use count
    "seq            bigint unsigned   " // sequence when use count was updated
    "};                                   ",

    "create index tlby on trustlines(by, currency, use);",
    "create index tlto on trustlines(to, currency, use);",

    "drop table exchanges;",

    "create table exchanges {             "
    "from           character(80),    "
    "to             character(80),    "
    "currency       character(80),    "
    "use            integer,          "
    "seq            bigint unsigned   "
    "};                                   ",

    "create index exby on exchanges(by, currency, use);",
    "create index exto on exchanges(to, currency, use);",
};

int pathfinddbcount = std::extent<decltype(pathfinddbinit)>::value;
*/

} // ripple
