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
#include <ripple/app/main/application.h>
#include <ripple/app/main/localcredentials.h>
#include <ripple/app/data/databasecon.h>
#include <ripple/app/data/sqlitedatabase.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/peers/clusternodestatus.h>
#include <ripple/app/peers/uniquenodelist.h>
#include <ripple/basics/log.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/basics/time.h>
#include <ripple/core/config.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/net/httpclient.h>
#include <beast/module/core/thread/deadlinetimer.h>
#include <beast/cxx14/memory.h> // <memory>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <fstream>

namespace ripple {

// xxx dynamically limit fetching by distance.
// xxx want a limit of 2000 validators.

// guarantees minimum throughput of 1 node per second.
#define node_fetch_jobs         10
#define node_fetch_seconds      10
#define node_file_bytes_max     (50<<10)    // 50k

// wait for validation information to be stable before scoring.
// #define score_delay_seconds      20
#define score_delay_seconds     5

// don't bother propagating past this number of rounds.
#define score_rounds            10

// vfalco todo replace macros with language constructs
#define validators_fetch_seconds    30
#define validators_file_bytes_max   (50 << 10)

// gather string constants.
#define section_currencies      "currencies"
#define section_domain          "domain"
#define section_ips             "ips"
#define section_ips_url         "ips_url"
#define section_public_key      "validation_public_key"
#define section_validators      "validators"
#define section_validators_url  "validators_url"

// limit pollution of database.
// yyy move to config file.
#define referral_validators_max 50
#define referral_ips_max        50

// vfalco todo move all function definitions inlined into the class.
class uniquenodelistimp
    : public uniquenodelist
    , public beast::deadlinetimer::listener
{
private:
    struct seeddomain
    {
        std::string                 strdomain;
        rippleaddress               napublickey;
        validatorsource             vssource;
        boost::posix_time::ptime    tpnext;
        boost::posix_time::ptime    tpscan;
        boost::posix_time::ptime    tpfetch;
        uint256                     isha256;
        std::string                 strcomment;
    };

    struct seednode
    {
        rippleaddress               napublickey;
        validatorsource             vssource;
        boost::posix_time::ptime    tpnext;
        boost::posix_time::ptime    tpscan;
        boost::posix_time::ptime    tpfetch;
        uint256                     isha256;
        std::string                 strcomment;
    };

    // used to distribute scores.
    struct scorenode
    {
        int                 iscore;
        int                 iroundscore;
        int                 iroundseed;
        int                 iseen;
        std::string         strvalidator;   // the public key.
        std::vector<int>    vireferrals;
    };

    typedef hash_map<std::string, int> strindex;
    typedef std::pair<std::string, int> ipandportnumber;
    typedef hash_map<std::pair< std::string, int>, score>   epscore;

public:
    explicit uniquenodelistimp (stoppable& parent)
        : uniquenodelist (parent)
        , m_scoretimer (this)
        , mfetchactive (0)
        , m_fetchtimer (this)
    {
        node_file_name_ = std::string (systemname ()) + ".txt";
        node_file_path_ = "/" + node_file_name_;
    }

    //--------------------------------------------------------------------------

    void onstop ()
    {
        m_fetchtimer.cancel ();
        m_scoretimer.cancel ();

        stopped ();
    }

    //--------------------------------------------------------------------------

    void doscore ()
    {
        mtpscorenext    = boost::posix_time::ptime (boost::posix_time::not_a_date_time); // timer not set.
        mtpscorestart   = boost::posix_time::second_clock::universal_time ();           // scoring.

        writelog (lstrace, uniquenodelist) << "scoring: start";

        scorecompute ();

        writelog (lstrace, uniquenodelist) << "scoring: end";

        // save update time.
        mtpscoreupdated = mtpscorestart;
        miscsave ();

        mtpscorestart   = boost::posix_time::ptime (boost::posix_time::not_a_date_time); // not scoring.

        // score again if needed.
        scorenext (false);
    }

    void dofetch ()
    {
        // time to check for another fetch.
        writelog (lstrace, uniquenodelist) << "fetchtimerhandler";
        fetchnext ();
    }

    void ondeadlinetimer (beast::deadlinetimer& timer)
    {
        if (timer == m_scoretimer)
        {
            getapp().getjobqueue ().addjob (jtunl, "unl.score",
                std::bind (&uniquenodelistimp::doscore, this));
        }
        else if (timer == m_fetchtimer)
        {
            getapp().getjobqueue ().addjob (jtunl, "unl.fetch",
                std::bind (&uniquenodelistimp::dofetch, this));
        }
    }

    //--------------------------------------------------------------------------

    // this is called when the application is started.
    // get update times and start fetching and scoring as needed.
    void start ()
    {
        miscload ();

        writelog (lsdebug, uniquenodelist) << "validator fetch updated: " << mtpfetchupdated;
        writelog (lsdebug, uniquenodelist) << "validator score updated: " << mtpscoreupdated;

        fetchnext ();           // start fetching.
        scorenext (false);      // start scoring.
    }

    //--------------------------------------------------------------------------

    // add a trusted node.  called by rpc or other source.
    void nodeaddpublic (rippleaddress const& nanodepublic, validatorsource vswhy, std::string const& strcomment)
    {
        seednode    sncurrent;

        bool        bfound      = getseednodes (nanodepublic, sncurrent);
        bool        bchanged    = false;

        if (!bfound)
        {
            sncurrent.napublickey   = nanodepublic;
            sncurrent.tpnext        = boost::posix_time::second_clock::universal_time ();
        }

        // promote source, if needed.
        if (!bfound /*|| isourcescore (vswhy) >= isourcescore (sncurrent.vssource)*/)
        {
            sncurrent.vssource      = vswhy;
            sncurrent.strcomment    = strcomment;
            bchanged                = true;
        }

        if (vsmanual == vswhy)
        {
            // a manual add forces immediate scan.
            sncurrent.tpnext        = boost::posix_time::second_clock::universal_time ();
            bchanged                = true;
        }

        if (bchanged)
            setseednodes (sncurrent, true);
    }

    //--------------------------------------------------------------------------

    // queue a domain for a single attempt fetch a ripple.txt.
    // --> strcomment: only used on vsmanual
    // yyy as a lot of these may happen at once, would be nice to wrap multiple calls in a transaction.
    void nodeadddomain (std::string strdomain, validatorsource vswhy, std::string const& strcomment)
    {
        boost::trim (strdomain);
        boost::to_lower (strdomain);

        // yyy would be best to verify strdomain is a valid domain.
        // writelog (lstrace) << str(boost::format("nodeadddomain: '%s' %c '%s'")
        //  % strdomain
        //  % vswhy
        //  % strcomment);

        seeddomain  sdcurrent;

        bool        bfound      = getseeddomains (strdomain, sdcurrent);
        bool        bchanged    = false;

        if (!bfound)
        {
            sdcurrent.strdomain     = strdomain;
            sdcurrent.tpnext        = boost::posix_time::second_clock::universal_time ();
        }

        // promote source, if needed.
        if (!bfound || isourcescore (vswhy) >= isourcescore (sdcurrent.vssource))
        {
            sdcurrent.vssource      = vswhy;
            sdcurrent.strcomment    = strcomment;
            bchanged                = true;
        }

        if (vsmanual == vswhy)
        {
            // a manual add forces immediate scan.
            sdcurrent.tpnext        = boost::posix_time::second_clock::universal_time ();
            bchanged                = true;
        }

        if (bchanged)
            setseeddomains (sdcurrent, true);
    }

    //--------------------------------------------------------------------------

    void noderemovepublic (rippleaddress const& nanodepublic)
    {
        {
            auto db = getapp().getwalletdb ().getdb ();
            auto sl (getapp().getwalletdb ().lock ());

            db->executesql (str (boost::format ("delete from seednodes where publickey=%s") % sqlescape (nanodepublic.humannodepublic ())));
            db->executesql (str (boost::format ("delete from trustednodes where publickey=%s") % sqlescape (nanodepublic.humannodepublic ())));
        }

        // yyy only dirty on successful delete.
        fetchdirty ();

        scopedunllocktype sl (munllock);
        munl.erase (nanodepublic.humannodepublic ());
    }

    //--------------------------------------------------------------------------

    void noderemovedomain (std::string strdomain)
    {
        boost::trim (strdomain);
        boost::to_lower (strdomain);

        {
            auto db = getapp().getwalletdb ().getdb ();
            auto sl (getapp().getwalletdb ().lock ());

            db->executesql (str (boost::format ("delete from seeddomains where domain=%s") % sqlescape (strdomain)));
        }

        // yyy only dirty on successful delete.
        fetchdirty ();
    }

    //--------------------------------------------------------------------------

    void nodereset ()
    {
        {
            auto db = getapp().getwalletdb ().getdb ();

            auto sl (getapp().getwalletdb ().lock ());

            // xxx check results.
            db->executesql ("delete from seeddomains");
            db->executesql ("delete from seednodes");
        }

        fetchdirty ();
    }

    //--------------------------------------------------------------------------

    // for debugging, schedule forced scoring.
    void nodescore ()
    {
        scorenext (true);
    }

    //--------------------------------------------------------------------------

    bool nodeinunl (rippleaddress const& nanodepublic)
    {
        scopedunllocktype sl (munllock);

        return munl.end () != munl.find (nanodepublic.humannodepublic ());
    }

    //--------------------------------------------------------------------------

    bool nodeincluster (rippleaddress const& nanodepublic)
    {
        scopedunllocktype sl (munllock);
        return m_clusternodes.end () != m_clusternodes.find (nanodepublic);
    }

    //--------------------------------------------------------------------------

    bool nodeincluster (rippleaddress const& nanodepublic, std::string& name)
    {
        scopedunllocktype sl (munllock);
        std::map<rippleaddress, clusternodestatus>::iterator it = m_clusternodes.find (nanodepublic);

        if (it == m_clusternodes.end ())
            return false;

        name = it->second.getname();
        return true;
    }

    //--------------------------------------------------------------------------

    bool nodeupdate (rippleaddress const& nanodepublic, clusternodestatus const& cnsstatus)
    {
        scopedunllocktype sl (munllock);
        return m_clusternodes[nanodepublic].update(cnsstatus);
    }

    //--------------------------------------------------------------------------

    std::map<rippleaddress, clusternodestatus> getclusterstatus ()
    {
        std::map<rippleaddress, clusternodestatus> ret;
        {
            scopedunllocktype sl (munllock);
            ret = m_clusternodes;
        }
        return ret;
    }

    //--------------------------------------------------------------------------

    std::uint32_t getclusterfee ()
    {
        int thresh = getapp().getops().getnetworktimenc() - 90;

        std::vector<std::uint32_t> fees;
        {
            scopedunllocktype sl (munllock);
            {
                for (std::map<rippleaddress, clusternodestatus>::iterator it = m_clusternodes.begin(),
                    end = m_clusternodes.end(); it != end; ++it)
                {
                    if (it->second.getreporttime() >= thresh)
                        fees.push_back(it->second.getloadfee());
                }
            }
        }

        if (fees.empty())
            return 0;
        std::sort (fees.begin(), fees.end());
        return fees[fees.size() / 2];
    }

    //--------------------------------------------------------------------------

    void addclusterstatus (json::value& obj)
    {
        scopedunllocktype sl (munllock);
        if (m_clusternodes.size() > 1) // nodes other than us
        {
            int          now   = getapp().getops().getnetworktimenc();
            std::uint32_t ref   = getapp().getfeetrack().getloadbase();
            json::value& nodes = (obj["cluster"] = json::objectvalue);

            for (std::map<rippleaddress, clusternodestatus>::iterator it = m_clusternodes.begin(),
                end = m_clusternodes.end(); it != end; ++it)
            {
                if (it->first != getapp().getlocalcredentials().getnodepublic())
                {
                    json::value& node = nodes[it->first.humannodepublic()];

                    if (!it->second.getname().empty())
                        node["tag"] = it->second.getname();

                    if ((it->second.getloadfee() != ref) && (it->second.getloadfee() != 0))
                        node["fee"] = static_cast<double>(it->second.getloadfee()) / ref;

                    if (it->second.getreporttime() != 0)
                        node["age"] = (it->second.getreporttime() >= now) ? 0 : (now - it->second.getreporttime());
                }
            }
        }
    }

    //--------------------------------------------------------------------------

    void nodebootstrap ()
    {
        int         idomains    = 0;
        int         inodes      = 0;

#if 0
        {
            auto sl (getapp().getwalletdb ().lock ());
            auto db = getapp().getwalletdb ().getdb ();

            if (db->executesql (str (boost::format ("select count(*) as count from seeddomains where source='%s' or source='%c';") % vsmanual % vsvalidator)) && db->startiterrows ())
                idomains    = db->getint ("count");

            db->enditerrows ();

            if (db->executesql (str (boost::format ("select count(*) as count from seednodes where source='%s' or source='%c';") % vsmanual % vsvalidator)) && db->startiterrows ())
                inodes      = db->getint ("count");

            db->enditerrows ();
        }
#endif

        bool    bloaded = idomains || inodes;

        // always merge in the file specified in the config.
        if (!getconfig ().validators_file.empty ())
        {
            writelog (lsinfo, uniquenodelist) << "bootstrapping unl: loading from unl_default.";

            bloaded = nodeload (getconfig ().validators_file);
        }

        // if never loaded anything try the current directory.
        if (!bloaded && getconfig ().validators_file.empty ())
        {
            writelog (lsinfo, uniquenodelist) << boost::str (boost::format ("bootstrapping unl: loading from '%s'.")
                                              % getconfig ().validators_base);

            bloaded = nodeload (getconfig ().validators_base);
        }

        // always load from rippled.cfg
        if (!getconfig ().validators.empty ())
        {
            rippleaddress   nainvalid;  // don't want a referrer on added entries.

            writelog (lsinfo, uniquenodelist) << boost::str (boost::format ("bootstrapping unl: loading from '%s'.")
                                              % getconfig ().config_file);

            if (processvalidators ("local", getconfig ().config_file.string (), nainvalid, vsconfig, &getconfig ().validators))
                bloaded = true;
        }

        if (!bloaded)
        {
            writelog (lsinfo, uniquenodelist) << boost::str (boost::format ("bootstrapping unl: loading from '%s'.")
                                              % getconfig ().validators_site);

            nodenetwork ();
        }
    }

    //--------------------------------------------------------------------------

    bool nodeload (boost::filesystem::path pconfig)
    {
        if (pconfig.empty ())
        {
            writelog (lsinfo, uniquenodelist) << config::helpers::getvalidatorsfilename() <<
                " path not specified.";

            return false;
        }

        if (!boost::filesystem::exists (pconfig))
        {
            writelog (lswarning, uniquenodelist) << config::helpers::getvalidatorsfilename() <<
                " not found: " << pconfig;

            return false;
        }

        if (!boost::filesystem::is_regular_file (pconfig))
        {
            writelog (lswarning, uniquenodelist) << config::helpers::getvalidatorsfilename() <<
                " not regular file: " << pconfig;

            return false;
        }

        std::ifstream   ifsdefault (pconfig.native ().c_str (), std::ios::in);

        if (!ifsdefault)
        {
            writelog (lsfatal, uniquenodelist) << config::helpers::getvalidatorsfilename() <<
                " failed to open: " << pconfig;

            return false;
        }

        std::string     strvalidators;

        strvalidators.assign ((std::istreambuf_iterator<char> (ifsdefault)),
                              std::istreambuf_iterator<char> ());

        if (ifsdefault.bad ())
        {
            writelog (lsfatal, uniquenodelist) << config::helpers::getvalidatorsfilename() <<
            "failed to read: " << pconfig;

            return false;
        }

        nodeprocess ("local", strvalidators, pconfig.string ());

        writelog (lstrace, uniquenodelist) << str (boost::format ("processing: %s") % pconfig);

        return true;
    }

    //--------------------------------------------------------------------------

    void nodenetwork ()
    {
        if (!getconfig ().validators_site.empty ())
        {
            httpclient::get (
                true,
                getapp().getioservice (),
                getconfig ().validators_site,
                443,
                getconfig ().validators_uri,
                validators_file_bytes_max,
                boost::posix_time::seconds (validators_fetch_seconds),
                std::bind (&uniquenodelistimp::validatorsresponse, this,
                           std::placeholders::_1,
                           std::placeholders::_2,
                           std::placeholders::_3));
        }
    }

    //--------------------------------------------------------------------------

    json::value getunljson ()
    {
        auto db = getapp().getwalletdb ().getdb ();

        json::value ret (json::arrayvalue);

        auto sl (getapp().getwalletdb ().lock ());
        sql_foreach (db, "select * from trustednodes;")
        {
            json::value node (json::objectvalue);

            node["publickey"]   = db->getstrbinary ("publickey");
            node["comment"]     = db->getstrbinary ("comment");

            ret.append (node);
        }

        return ret;
    }

    //--------------------------------------------------------------------------

    // for each kind of source, have a starting number of points to be distributed.
    int isourcescore (validatorsource vswhy)
    {
        int     iscore  = 0;

        switch (vswhy)
        {
        case vsconfig:
            iscore  = 1500;
            break;

        case vsinbound:
            iscore  =    0;
            break;

        case vsmanual:
            iscore  = 1500;
            break;

        case vsreferral:
            iscore  =    0;
            break;

        case vstold:
            iscore  =    0;
            break;

        case vsvalidator:
            iscore  = 1000;
            break;

        case vsweb:
            iscore  =  200;
            break;

        default:
            throw std::runtime_error ("internal error: bad validatorsource.");
        }

        return iscore;
    }

    //--------------------------------------------------------------------------
private:
    // load information about when we last updated.
    bool miscload ()
    {
        auto sl (getapp().getwalletdb ().lock ());
        auto db = getapp().getwalletdb ().getdb ();

        if (!db->executesql ("select * from misc where magic=1;"))
            return false;

        bool const bavail  = db->startiterrows ();

        mtpfetchupdated = ptfromseconds (bavail
            ? db->getint ("fetchupdated")
            : -1);
        mtpscoreupdated = ptfromseconds (bavail
            ? db->getint ("scoreupdated")
            : -1);

        db->enditerrows ();

        trustedload ();

        return true;
    }

    //--------------------------------------------------------------------------

    // persist update information.
    bool miscsave ()
    {
        auto db = getapp().getwalletdb ().getdb ();
        auto sl (getapp().getwalletdb ().lock ());

        db->executesql (str (boost::format ("replace into misc (magic,fetchupdated,scoreupdated) values (1,%d,%d);")
                             % itoseconds (mtpfetchupdated)
                             % itoseconds (mtpscoreupdated)));

        return true;
    }

    //--------------------------------------------------------------------------

    void trustedload ()
    {
        boost::regex rnode ("\\`\\s*(\\s+)[\\s]*(.*)\\'");
        boost_foreach (std::string const& c, getconfig ().cluster_nodes)
        {
            boost::smatch match;

            if (boost::regex_match (c, match, rnode))
            {
                rippleaddress a = rippleaddress::createnodepublic (match[1]);

                if (a.isvalid ())
                    m_clusternodes.insert (std::make_pair (a, clusternodestatus(match[2])));
            }
            else
                writelog (lswarning, uniquenodelist) << "entry in cluster list invalid: '" << c << "'";
        }

        auto db = getapp().getwalletdb ().getdb ();
        auto sl (getapp().getwalletdb ().lock ());
        scopedunllocktype slunl (munllock);

        munl.clear ();

        // xxx needs to limit by quanity and quality.
        sql_foreach (db, "select publickey from trustednodes where score != 0;")
        {
            munl.insert (db->getstrbinary ("publickey"));
        }
    }

    //--------------------------------------------------------------------------

    // for a round of scoring we destribute points from a node to nodes it refers to.
    // returns true, iff scores were distributed.
    //
    bool scoreround (std::vector<scorenode>& vsnnodes)
    {
        bool    bdist   = false;

        // for each node, distribute roundseed to roundscores.
        boost_foreach (scorenode & sn, vsnnodes)
        {
            int     ientries    = sn.vireferrals.size ();

            if (sn.iroundseed && ientries)
            {
                score   itotal  = (ientries + 1) * ientries / 2;
                score   ibase   = sn.iroundseed * ientries / itotal;

                // distribute the current entires' seed score to validators prioritized by mention order.
                for (int i = 0; i != ientries; i++)
                {
                    score   ipoints = ibase * (ientries - i) / ientries;

                    vsnnodes[sn.vireferrals[i]].iroundscore += ipoints;
                }
            }
        }

        if (shouldlog (lstrace, uniquenodelist))
        {
            writelog (lstrace, uniquenodelist) << "midway: ";
            boost_foreach (scorenode & sn, vsnnodes)
            {
                writelog (lstrace, uniquenodelist) << str (boost::format ("%s| %d, %d, %d: [%s]")
                                                   % sn.strvalidator
                                                   % sn.iscore
                                                   % sn.iroundscore
                                                   % sn.iroundseed
                                                   % strjoin (sn.vireferrals.begin (), sn.vireferrals.end (), ","));
            }
        }

        // add roundscore to score.
        // make roundscore new roundseed.
        boost_foreach (scorenode & sn, vsnnodes)
        {
            if (!bdist && sn.iroundscore)
                bdist   = true;

            sn.iscore       += sn.iroundscore;
            sn.iroundseed   = sn.iroundscore;
            sn.iroundscore  = 0;
        }

        if (shouldlog (lstrace, uniquenodelist))
        {
            writelog (lstrace, uniquenodelist) << "finish: ";
            boost_foreach (scorenode & sn, vsnnodes)
            {
                writelog (lstrace, uniquenodelist) << str (boost::format ("%s| %d, %d, %d: [%s]")
                                                   % sn.strvalidator
                                                   % sn.iscore
                                                   % sn.iroundscore
                                                   % sn.iroundseed
                                                   % strjoin (sn.vireferrals.begin (), sn.vireferrals.end (), ","));
            }
        }

        return bdist;
    }

    //--------------------------------------------------------------------------

    // from seeddomains and validatorreferrals compute scores and update trustednodes.
    //
    // vfalco todo shrink this function, break it up
    //
    void scorecompute ()
    {
        strindex                umpulicidx;     // map of public key to index.
        strindex                umdomainidx;    // map of domain to index.
        std::vector<scorenode>  vsnnodes;       // index to scoring node.

        auto db = getapp().getwalletdb ().getdb ();

        // for each entry in seeddomains with a publickey:
        // - add an entry in umpulicidx, umdomainidx, and vsnnodes.
        {
            auto sl (getapp().getwalletdb ().lock ());

            sql_foreach (db, "select domain,publickey,source from seeddomains;")
            {
                if (db->getnull ("publickey"))
                {
                    // we ignore entries we don't have public keys for.
                }
                else
                {
                    std::string strdomain       = db->getstrbinary ("domain");
                    std::string strpublickey    = db->getstrbinary ("publickey");
                    std::string strsource       = db->getstrbinary ("source");
                    int         iscore          = isourcescore (static_cast<validatorsource> (strsource[0]));
                    strindex::iterator  siold   = umpulicidx.find (strpublickey);

                    if (siold == umpulicidx.end ())
                    {
                        // new node
                        int         inode       = vsnnodes.size ();

                        umpulicidx[strpublickey]    = inode;
                        umdomainidx[strdomain]      = inode;

                        scorenode   sncurrent;

                        sncurrent.strvalidator  = strpublickey;
                        sncurrent.iscore        = iscore;
                        sncurrent.iroundseed    = sncurrent.iscore;
                        sncurrent.iroundscore   = 0;
                        sncurrent.iseen         = -1;

                        vsnnodes.push_back (sncurrent);
                    }
                    else
                    {
                        scorenode&  snold   = vsnnodes[siold->second];

                        if (snold.iscore < iscore)
                        {
                            // update old node

                            snold.iscore        = iscore;
                            snold.iroundseed    = snold.iscore;
                        }
                    }
                }
            }
        }

        // for each entry in seednodes:
        // - add an entry in umpulicidx, umdomainidx, and vsnnodes.
        {
            auto sl (getapp().getwalletdb ().lock ());

            sql_foreach (db, "select publickey,source from seednodes;")
            {
                std::string strpublickey    = db->getstrbinary ("publickey");
                std::string strsource       = db->getstrbinary ("source");
                int         iscore          = isourcescore (static_cast<validatorsource> (strsource[0]));
                strindex::iterator  siold   = umpulicidx.find (strpublickey);

                if (siold == umpulicidx.end ())
                {
                    // new node
                    int         inode       = vsnnodes.size ();

                    umpulicidx[strpublickey]    = inode;

                    scorenode   sncurrent;

                    sncurrent.strvalidator  = strpublickey;
                    sncurrent.iscore        = iscore;
                    sncurrent.iroundseed    = sncurrent.iscore;
                    sncurrent.iroundscore   = 0;
                    sncurrent.iseen         = -1;

                    vsnnodes.push_back (sncurrent);
                }
                else
                {
                    scorenode&  snold   = vsnnodes[siold->second];

                    if (snold.iscore < iscore)
                    {
                        // update old node

                        snold.iscore        = iscore;
                        snold.iroundseed    = snold.iscore;
                    }
                }
            }
        }

        // for debugging, print out initial scores.
        if (shouldlog (lstrace, uniquenodelist))
        {
            boost_foreach (scorenode & sn, vsnnodes)
            {
                writelog (lstrace, uniquenodelist) << str (boost::format ("%s| %d, %d, %d")
                                                   % sn.strvalidator
                                                   % sn.iscore
                                                   % sn.iroundscore
                                                   % sn.iroundseed);
            }
        }

        // writelog (lstrace, uniquenodelist) << str(boost::format("vsnnodes.size=%d") % vsnnodes.size());

        // step through growing list of nodes adding each validation list.
        // - each validator may have provided referals.  add those referals as validators.
        for (int inode = 0; inode != vsnnodes.size (); ++inode)
        {
            scorenode&          sn              = vsnnodes[inode];
            std::string&        strvalidator    = sn.strvalidator;
            std::vector<int>&   vireferrals     = sn.vireferrals;

            auto sl (getapp().getwalletdb ().lock ());

            sql_foreach (db, boost::str (boost::format ("select referral from validatorreferrals where validator=%s order by entry;")
                                         % sqlescape (strvalidator)))
            {
                std::string strreferral = db->getstrbinary ("referral");
                int         ireferral;

                strindex::iterator  itentry;

                rippleaddress       na;

                if (na.setnodepublic (strreferral))
                {
                    // referring a public key.
                    itentry     = umpulicidx.find (strreferral);

                    if (itentry == umpulicidx.end ())
                    {
                        // not found add public key to list of nodes.
                        ireferral               = vsnnodes.size ();

                        umpulicidx[strreferral] = ireferral;

                        scorenode   sncurrent;

                        sncurrent.strvalidator  = strreferral;
                        sncurrent.iscore        = isourcescore (vsreferral);
                        sncurrent.iroundseed    = sncurrent.iscore;
                        sncurrent.iroundscore   = 0;
                        sncurrent.iseen         = -1;

                        vsnnodes.push_back (sncurrent);
                    }
                    else
                    {
                        ireferral   =  itentry->second;
                    }

                    // writelog (lstrace, uniquenodelist) << str(boost::format("%s: public=%s ireferral=%d") % strvalidator % strreferral % ireferral);

                }
                else
                {
                    // referring a domain.
                    itentry     = umdomainidx.find (strreferral);
                    ireferral   = itentry == umdomainidx.end ()
                                  ? -1            // we ignore domains we can't find entires for.
                                  : itentry->second;

                    // writelog (lstrace, uniquenodelist) << str(boost::format("%s: domain=%s ireferral=%d") % strvalidator % strreferral % ireferral);
                }

                if (ireferral >= 0 && inode != ireferral)
                    vireferrals.push_back (ireferral);
            }
        }

        //
        // distribute the points from the seeds.
        //
        bool    bdist   = true;

        for (int i = score_rounds; bdist && i--;)
            bdist   = scoreround (vsnnodes);

        if (shouldlog (lstrace, uniquenodelist))
        {
            writelog (lstrace, uniquenodelist) << "scored:";
            boost_foreach (scorenode & sn, vsnnodes)
            {
                writelog (lstrace, uniquenodelist) << str (boost::format ("%s| %d, %d, %d: [%s]")
                                                   % sn.strvalidator
                                                   % sn.iscore
                                                   % sn.iroundscore
                                                   % sn.iroundseed
                                                   % strjoin (sn.vireferrals.begin (), sn.vireferrals.end (), ","));
            }
        }

        // persist validator scores.
        auto sl (getapp().getwalletdb ().lock ());

        db->executesql ("begin;");
        db->executesql ("update trustednodes set score = 0 where score != 0;");

        if (!vsnnodes.empty ())
        {
            // load existing seens from db.
            std::vector<std::string>    vstrpublickeys;

            vstrpublickeys.resize (vsnnodes.size ());

            for (int inode = vsnnodes.size (); inode--;)
            {
                vstrpublickeys[inode]   = sqlescape (vsnnodes[inode].strvalidator);
            }

            sql_foreach (db, str (boost::format ("select publickey,seen from trustednodes where publickey in (%s);")
                                  % strjoin (vstrpublickeys.begin (), vstrpublickeys.end (), ",")))
            {
                vsnnodes[umpulicidx[db->getstrbinary ("publickey")]].iseen   = db->getnull ("seen") ? -1 : db->getint ("seen");
            }
        }

        hash_set<std::string>   usunl;

        if (!vsnnodes.empty ())
        {
            // update the score old entries and add new entries as needed.
            std::vector<std::string>    vstrvalues;

            vstrvalues.resize (vsnnodes.size ());

            for (int inode = vsnnodes.size (); inode--;)
            {
                scorenode&  sn      = vsnnodes[inode];
                std::string strseen = sn.iseen >= 0 ? str (boost::format ("%d") % sn.iseen) : "null";

                vstrvalues[inode]   = str (boost::format ("(%s,%s,%s)")
                                           % sqlescape (sn.strvalidator)
                                           % sn.iscore
                                           % strseen);

                usunl.insert (sn.strvalidator);
            }

            db->executesql (str (boost::format ("replace into trustednodes (publickey,score,seen) values %s;")
                                 % strjoin (vstrvalues.begin (), vstrvalues.end (), ",")));
        }

        {
            scopedunllocktype sl (munllock);

            // xxx should limit to scores above a certain minimum and limit to a certain number.
            munl.swap (usunl);
        }

        hash_map<std::string, int>  umvalidators;

        if (!vsnnodes.empty ())
        {
            std::vector<std::string> vstrpublickeys;

            // for every ipreferral add a score for the ip and port.
            sql_foreach (db, "select validator,count(*) as count from ipreferrals group by validator;")
            {
                umvalidators[db->getstrbinary ("validator")] = db->getint ("count");

                // writelog (lstrace, uniquenodelist) << strvalidator << ":" << db->getint("count");
            }
        }

        // for each validator, get each referral and add its score to ip's score.
        // map of pair<ip,port> :: score
        epscore umscore;

        typedef hash_map<std::string, int>::value_type vctype;
        boost_foreach (vctype & vc, umvalidators)
        {
            std::string strvalidator    = vc.first;

            strindex::iterator  itindex = umpulicidx.find (strvalidator);

            if (itindex != umpulicidx.end ())
            {
                int         iseed           = vsnnodes[itindex->second].iscore;
                int         ientries        = vc.second;
                score       itotal          = (ientries + 1) * ientries / 2;
                score       ibase           = iseed * ientries / itotal;
                int         ientry          = 0;

                sql_foreach (db, str (boost::format ("select ip,port from ipreferrals where validator=%s order by entry;")
                                      % sqlescape (strvalidator)))
                {
                    score       ipoints = ibase * (ientries - ientry) / ientries;
                    int         iport;

                    iport       = db->getnull ("port") ? -1 : db->getint ("port");

                    std::pair< std::string, int>    ep  = std::make_pair (db->getstrbinary ("ip"), iport);

                    epscore::iterator   itep    = umscore.find (ep);

                    umscore[ep] = itep == umscore.end () ? ipoints :  itep->second + ipoints;
                    ientry++;
                }
            }
        }

        db->executesql ("commit;");
    }

    //--------------------------------------------------------------------------

    // start a timer to update scores.
    // <-- bnow: true, to force scoring for debugging.
    void scorenext (bool bnow)
    {
        // writelog (lstrace, uniquenodelist) << str(boost::format("scorenext: mtpfetchupdated=%s mtpscorestart=%s mtpscoreupdated=%s mtpscorenext=%s") % mtpfetchupdated % mtpscorestart % mtpscoreupdated % mtpscorenext);
        bool    bcanscore   = mtpscorestart.is_not_a_date_time ()                           // not scoring.
                              && !mtpfetchupdated.is_not_a_date_time ();                  // something to score.

        bool    bdirty  =
            (mtpscoreupdated.is_not_a_date_time () || mtpscoreupdated <= mtpfetchupdated)   // not already scored.
            && (mtpscorenext.is_not_a_date_time ()                                          // timer is not fine.
                || mtpscorenext < mtpfetchupdated + boost::posix_time::seconds (score_delay_seconds));

        if (bcanscore && (bnow || bdirty))
        {
            // need to update or set timer.
            double const secondsfromnow = bnow ? 0 : score_delay_seconds;
            mtpscorenext = boost::posix_time::second_clock::universal_time ()                   // past now too.
                           + boost::posix_time::seconds (secondsfromnow);

            // writelog (lstrace, uniquenodelist) << str(boost::format("scorenext: @%s") % mtpscorenext);
            m_scoretimer.setexpiration (secondsfromnow);
        }
    }

    //--------------------------------------------------------------------------

    // given a ripple.txt, process it.
    //
    // vfalco todo can't we take a filename or stream instead of a string?
    //
    bool responsefetch (std::string const& strdomain, const boost::system::error_code& err, int istatus, std::string const& strsitefile)
    {
        bool    breject = !err && istatus != 200;

        if (!breject)
        {
            inifilesections secsite = parseinifile (strsitefile, true);
            bool bgood   = !err;

            if (bgood)
            {
                writelog (lstrace, uniquenodelist) << strdomain
                    << ": retrieved configuration";
            }
            else
            {
                writelog (lstrace, uniquenodelist) << strdomain
                    << ": unable to retrieve configuration: "
                    <<  err.message ();
            }

            //
            // verify file domain
            //
            std::string strsite;

            if (bgood && !getsinglesection (secsite, section_domain, strsite))
            {
                bgood   = false;

                writelog (lstrace, uniquenodelist) << strdomain
                    << ": " << section_domain
                    << "entry missing.";
            }

            if (bgood && strsite != strdomain)
            {
                bgood   = false;

                writelog (lstrace, uniquenodelist) << strdomain
                    << ": " << section_domain << " does not match " << strsite;
            }

            //
            // process public key
            //
            std::string     strnodepublickey;

            if (bgood && !getsinglesection (secsite, section_public_key, strnodepublickey))
            {
                // bad [validation_public_key] inifilesections.
                bgood   = false;

                writelog (lstrace, uniquenodelist) << strdomain
                    << ": " << section_public_key << " entry missing.";
            }

            rippleaddress   nanodepublic;

            if (bgood && !nanodepublic.setnodepublic (strnodepublickey))
            {
                // bad public key.
                bgood   = false;

                writelog (lstrace, uniquenodelist) << strdomain
                    << ": " << section_public_key << " is not a public key: "
                    << strnodepublickey;
            }

            if (bgood)
            {
                seeddomain  sdcurrent;

                bool bfound = getseeddomains (strdomain, sdcurrent);
                assert (bfound);
                (void) bfound;

                uint256     isha256     = serializer::getsha512half (strsitefile);
                bool        bchangedb   = sdcurrent.isha256 != isha256;

                sdcurrent.strdomain     = strdomain;
                // xxx if the node public key is changing, delete old public key information?
                // xxx only if no other refs to keep it arround, other wise we have an attack vector.
                sdcurrent.napublickey   = nanodepublic;

                // writelog (lstrace, uniquenodelist) << boost::format("sdcurrent.napublickey: '%s'") % sdcurrent.napublickey.humannodepublic();

                sdcurrent.tpfetch       = boost::posix_time::second_clock::universal_time ();
                sdcurrent.isha256       = isha256;

                setseeddomains (sdcurrent, true);

                if (bchangedb)
                {
                    writelog (lstrace, uniquenodelist) << strdomain
                        << ": processing new " << node_file_name_ << ".";
                    processfile (strdomain, nanodepublic, secsite);
                }
                else
                {
                    writelog (lstrace, uniquenodelist) << strdomain
                        << ": no change in " << node_file_name_ << ".";
                    fetchfinish ();
                }
            }
            else
            {
                // failed: update

                // xxx if we have public key, perhaps try look up in cas?
                fetchfinish ();
            }
        }

        return breject;
    }

    //--------------------------------------------------------------------------

    // try to process the next fetch of a ripple.txt.
    void fetchnext ()
    {
        bool    bfull;

        {
            scopedfetchlocktype sl (mfetchlock);

            bfull = (mfetchactive == node_fetch_jobs);
        }

        if (!bfull)
        {
            // determine next scan.
            std::string strdomain;
            boost::posix_time::ptime tpnext (boost::posix_time::min_date_time);
            boost::posix_time::ptime tpnow (boost::posix_time::second_clock::universal_time ());

            auto sl (getapp().getwalletdb ().lock ());
            auto db = getapp().getwalletdb ().getdb ();

            if (db->executesql ("select domain,next from seeddomains indexed by seeddomainnext order by next limit 1;")
                    && db->startiterrows ())
            {
                int inext (db->getint ("next"));

                tpnext  = ptfromseconds (inext);

                writelog (lstrace, uniquenodelist) << str (boost::format ("fetchnext: inext=%s tpnext=%s tpnow=%s") % inext % tpnext % tpnow);
                strdomain = db->getstrbinary ("domain");

                db->enditerrows ();
            }

            if (!strdomain.empty ())
            {
                scopedfetchlocktype sl (mfetchlock);

                bfull = (mfetchactive == node_fetch_jobs);

                if (!bfull && tpnext <= tpnow)
                {
                    mfetchactive++;
                }
            }

            if (strdomain.empty () || bfull)
            {
                writelog (lstrace, uniquenodelist) << str (boost::format ("fetchnext: strdomain=%s bfull=%d") % strdomain % bfull);
            }
            else if (tpnext > tpnow)
            {
                writelog (lstrace, uniquenodelist) << str (boost::format ("fetchnext: set timer : strdomain=%s") % strdomain);
                // fetch needs to happen in the future.  set a timer to wake us.
                mtpfetchnext    = tpnext;

                double seconds = (tpnext - tpnow).seconds ();

                // vfalco check this.
                if (seconds == 0)
                    seconds = 1;

                m_fetchtimer.setexpiration (seconds);
            }
            else
            {
                writelog (lstrace, uniquenodelist) << str (boost::format ("fetchnext: fetch now: strdomain=%s tpnext=%s tpnow=%s") % strdomain % tpnext % tpnow);
                // fetch needs to happen now.
                mtpfetchnext    = boost::posix_time::ptime (boost::posix_time::not_a_date_time);

                seeddomain  sdcurrent;
                bool bfound  = getseeddomains (strdomain, sdcurrent);
                assert (bfound);
                (void) bfound;

                // update time of next fetch and this scan attempt.
                sdcurrent.tpscan        = tpnow;

                // xxx use a longer duration if we have lots of validators.
                sdcurrent.tpnext        = sdcurrent.tpscan + boost::posix_time::hours (7 * 24);

                setseeddomains (sdcurrent, false);

                writelog (lstrace, uniquenodelist) << strdomain
                    << " fetching " << node_file_name_ << ".";

                fetchprocess (strdomain);   // go get it.

                fetchnext ();               // look for more.
            }
        }
    }

    //--------------------------------------------------------------------------

    // called when we need to update scores.
    void fetchdirty ()
    {
        // note update.
        mtpfetchupdated = boost::posix_time::second_clock::universal_time ();
        miscsave ();

        // update scores.
        scorenext (false);
    }


    //--------------------------------------------------------------------------

    void fetchfinish ()
    {
        {
            scopedfetchlocktype sl (mfetchlock);
            mfetchactive--;
        }

        fetchnext ();
    }

    //--------------------------------------------------------------------------

    // get the ripple.txt and process it.
    void fetchprocess (std::string strdomain)
    {
        writelog (lstrace, uniquenodelist) << strdomain
            << ": fetching " << node_file_name_ << ".";

        std::deque<std::string> deqsites;

        // order searching from most specifically for purpose to generic.
        // this order allows the client to take the most burden rather than the servers.
        deqsites.push_back (systemname () + strdomain);
        deqsites.push_back ("www." + strdomain);
        deqsites.push_back (strdomain);

        httpclient::get (
            true,
            getapp().getioservice (),
            deqsites,
            443,
            node_file_path_,
            node_file_bytes_max,
            boost::posix_time::seconds (node_fetch_seconds),
            std::bind (&uniquenodelistimp::responsefetch, this, strdomain,
                       std::placeholders::_1, std::placeholders::_2,
                       std::placeholders::_3));
    }

    //--------------------------------------------------------------------------

    void fetchtimerhandler (const boost::system::error_code& err)
    {
        if (!err)
        {
            ondeadlinetimer (m_fetchtimer);
        }
    }


    //--------------------------------------------------------------------------

    // process inifilesections [validators_url].
    void getvalidatorsurl (rippleaddress const& nanodepublic,
        inifilesections secsite)
    {
        std::string strvalidatorsurl;
        std::string strscheme;
        std::string strdomain;
        int         iport;
        std::string strpath;

        if (getsinglesection (secsite, section_validators_url, strvalidatorsurl)
                && !strvalidatorsurl.empty ()
                && parseurl (strvalidatorsurl, strscheme, strdomain, iport, strpath)
                && -1 == iport
                && strscheme == "https")
        {
            httpclient::get (
                true,
                getapp().getioservice (),
                strdomain,
                443,
                strpath,
                node_file_bytes_max,
                boost::posix_time::seconds (node_fetch_seconds),
                std::bind (&uniquenodelistimp::responsevalidators, this,
                           strvalidatorsurl, nanodepublic, secsite, strdomain,
                           std::placeholders::_1, std::placeholders::_2,
                           std::placeholders::_3));
        }
        else
        {
            getipsurl (nanodepublic, secsite);
        }
    }

    //--------------------------------------------------------------------------

    // process inifilesections [ips_url].
    // if we have a inifilesections with a single entry, fetch the url and process it.
    void getipsurl (rippleaddress const& nanodepublic, inifilesections secsite)
    {
        std::string stripsurl;
        std::string strscheme;
        std::string strdomain;
        int         iport;
        std::string strpath;

        if (getsinglesection (secsite, section_ips_url, stripsurl)
                && !stripsurl.empty ()
                && parseurl (stripsurl, strscheme, strdomain, iport, strpath)
                && -1 == iport
                && strscheme == "https")
        {
            httpclient::get (
                true,
                getapp().getioservice (),
                strdomain,
                443,
                strpath,
                node_file_bytes_max,
                boost::posix_time::seconds (node_fetch_seconds),
                std::bind (&uniquenodelistimp::responseips, this, strdomain,
                           nanodepublic, std::placeholders::_1,
                           std::placeholders::_2, std::placeholders::_3));
        }
        else
        {
            fetchfinish ();
        }
    }


    //--------------------------------------------------------------------------

    // given a inifilesections with ips, parse and persist it for a validator.
    bool responseips (std::string const& strsite, rippleaddress const& nanodepublic, const boost::system::error_code& err, int istatus, std::string const& stripsfile)
    {
        bool    breject = !err && istatus != 200;

        if (!breject)
        {
            if (!err)
            {
                inifilesections         secfile     = parseinifile (stripsfile, true);

                processips (strsite, nanodepublic, getinifilesection (secfile, section_ips));
            }

            fetchfinish ();
        }

        return breject;
    }

    // after fetching a ripple.txt from a web site, given a inifilesections with validators, parse and persist it.
    bool responsevalidators (std::string const& strvalidatorsurl, rippleaddress const& nanodepublic, inifilesections secsite, std::string const& strsite, const boost::system::error_code& err, int istatus, std::string const& strvalidatorsfile)
    {
        bool    breject = !err && istatus != 200;

        if (!breject)
        {
            if (!err)
            {
                inifilesections     secfile     = parseinifile (strvalidatorsfile, true);

                processvalidators (strsite, strvalidatorsurl, nanodepublic, vsvalidator, getinifilesection (secfile, section_validators));
            }

            getipsurl (nanodepublic, secsite);
        }

        return breject;
    }


    //--------------------------------------------------------------------------

    // persist the ips refered to by a validator.
    // --> strsite: source of the ips (for debugging)
    // --> nanodepublic: public key of the validating node.
    void processips (std::string const& strsite, rippleaddress const& nanodepublic, inifilesections::mapped_type* pmtvecstrips)
    {
        auto db = getapp().getwalletdb ().getdb ();

        std::string strescnodepublic    = sqlescape (nanodepublic.humannodepublic ());

        writelog (lsdebug, uniquenodelist)
                << str (boost::format ("validator: '%s' processing %d ips.")
                        % strsite % ( pmtvecstrips ? pmtvecstrips->size () : 0));

        // remove all current validator's entries in ipreferrals
        {
            auto sl (getapp().getwalletdb ().lock ());
            db->executesql (str (boost::format ("delete from ipreferrals where validator=%s;") % strescnodepublic));
            // xxx check result.
        }

        // add new referral entries.
        if (pmtvecstrips && !pmtvecstrips->empty ())
        {
            std::vector<std::string>    vstrvalues;

            vstrvalues.resize (std::min ((int) pmtvecstrips->size (), referral_ips_max));

            int ivalues = 0;
            boost_foreach (std::string const& strreferral, *pmtvecstrips)
            {
                if (ivalues == referral_validators_max)
                    break;

                std::string     strip;
                int             iport;
                bool            bvalid  = parseipport (strreferral, strip, iport);

                // xxx filter out private network ips.
                // xxx http://en.wikipedia.org/wiki/private_network

                if (bvalid)
                {
                    vstrvalues[ivalues] = str (boost::format ("(%s,%d,%s,%d)")
                                               % strescnodepublic % ivalues % sqlescape (strip) % iport);
                    ivalues++;
                }
                else
                {
                    writelog (lstrace, uniquenodelist)
                            << str (boost::format ("validator: '%s' [" section_ips "]: rejecting '%s'")
                                    % strsite % strreferral);
                }
            }

            if (ivalues)
            {
                vstrvalues.resize (ivalues);

                auto sl (getapp().getwalletdb ().lock ());
                db->executesql (str (boost::format ("insert into ipreferrals (validator,entry,ip,port) values %s;")
                                     % strjoin (vstrvalues.begin (), vstrvalues.end (), ",")));
                // xxx check result.
            }
        }

        fetchdirty ();
    }

    //--------------------------------------------------------------------------

    // persist validatorreferrals.
    // --> strsite: source site for display
    // --> strvalidatorssrc: source details for display
    // --> nanodepublic: remote source public key - not valid for local
    // --> vswhy: reason for adding validator to seeddomains or seednodes.
    int processvalidators (std::string const& strsite, std::string const& strvalidatorssrc, rippleaddress const& nanodepublic, validatorsource vswhy, inifilesections::mapped_type* pmtvecstrvalidators)
    {
        auto db              = getapp().getwalletdb ().getdb ();
        std::string strnodepublic   = nanodepublic.isvalid () ? nanodepublic.humannodepublic () : strvalidatorssrc;
        int         ivalues         = 0;

        writelog (lstrace, uniquenodelist)
                << str (boost::format ("validator: '%s' : '%s' : processing %d validators.")
                        % strsite
                        % strvalidatorssrc
                        % ( pmtvecstrvalidators ? pmtvecstrvalidators->size () : 0));

        // remove all current validator's entries in validatorreferrals
        {
            auto sl (getapp().getwalletdb ().lock ());

            db->executesql (str (boost::format ("delete from validatorreferrals where validator='%s';") % strnodepublic));
            // xxx check result.
        }

        // add new referral entries.
        if (pmtvecstrvalidators && pmtvecstrvalidators->size ())
        {
            std::vector<std::string> vstrvalues;

            vstrvalues.reserve (std::min ((int) pmtvecstrvalidators->size (), referral_validators_max));

            boost_foreach (std::string const& strreferral, *pmtvecstrvalidators)
            {
                if (ivalues == referral_validators_max)
                    break;

                boost::smatch   smmatch;

                // domain comment?
                // public_key comment?
                static boost::regex rereferral ("\\`\\s*(\\s+)(?:\\s+(.+))?\\s*\\'");

                if (!boost::regex_match (strreferral, smmatch, rereferral))
                {
                    writelog (lswarning, uniquenodelist) << str (boost::format ("bad validator: syntax error: %s: %s") % strsite % strreferral);
                }
                else
                {
                    std::string     strrefered  = smmatch[1];
                    std::string     strcomment  = smmatch[2];
                    rippleaddress   navalidator;

                    if (navalidator.setseedgeneric (strrefered))
                    {
                        writelog (lswarning, uniquenodelist) << str (boost::format ("bad validator: domain or public key required: %s %s") % strrefered % strcomment);
                    }
                    else if (navalidator.setnodepublic (strrefered))
                    {
                        // a public key.
                        // xxx schedule for cas lookup.
                        nodeaddpublic (navalidator, vswhy, strcomment);

                        writelog (lsinfo, uniquenodelist) << str (boost::format ("node public: %s %s") % strrefered % strcomment);

                        if (nanodepublic.isvalid ())
                            vstrvalues.push_back (str (boost::format ("('%s',%d,'%s')") % strnodepublic % ivalues % navalidator.humannodepublic ()));

                        ivalues++;
                    }
                    else
                    {
                        // a domain: need to look it up.
                        nodeadddomain (strrefered, vswhy, strcomment);

                        writelog (lsinfo, uniquenodelist) << str (boost::format ("node domain: %s %s") % strrefered % strcomment);

                        if (nanodepublic.isvalid ())
                            vstrvalues.push_back (str (boost::format ("('%s',%d,%s)") % strnodepublic % ivalues % sqlescape (strrefered)));

                        ivalues++;
                    }
                }
            }

            if (!vstrvalues.empty ())
            {
                std::string strsql  = str (boost::format ("insert into validatorreferrals (validator,entry,referral) values %s;")
                                           % strjoin (vstrvalues.begin (), vstrvalues.end (), ","));

                auto sl (getapp().getwalletdb ().lock ());

                db->executesql (strsql);
                // xxx check result.
            }
        }

        fetchdirty ();

        return ivalues;
    }

    //--------------------------------------------------------------------------

    // process a ripple.txt.
    void processfile (std::string const& strdomain, rippleaddress const& nanodepublic, inifilesections secsite)
    {
        //
        // process validators
        //
        processvalidators (strdomain, node_file_name_, nanodepublic,
            vsreferral, getinifilesection (secsite, section_validators));

        //
        // process ips
        //
        processips (strdomain, nanodepublic, getinifilesection (secsite, section_ips));

        //
        // process currencies
        //
        inifilesections::mapped_type*   pvcurrencies;

        if ((pvcurrencies = getinifilesection (secsite, section_currencies)) && pvcurrencies->size ())
        {
            // xxx process currencies.
            writelog (lswarning, uniquenodelist) << "ignoring currencies: not implemented.";
        }

        getvalidatorsurl (nanodepublic, secsite);
    }

    //--------------------------------------------------------------------------

    // retrieve a seeddomain from db.
    bool getseeddomains (std::string const& strdomain, seeddomain& dstseeddomain)
    {
        bool        bresult;
        auto db = getapp().getwalletdb ().getdb ();

        std::string strsql  = boost::str (boost::format ("select * from seeddomains where domain=%s;")
                                          % sqlescape (strdomain));

        auto sl (getapp().getwalletdb ().lock ());

        bresult = db->executesql (strsql) && db->startiterrows ();

        if (bresult)
        {
            std::string     strpublickey;
            int             inext;
            int             iscan;
            int             ifetch;
            std::string     strsha256;

            dstseeddomain.strdomain = db->getstrbinary ("domain");

            if (!db->getnull ("publickey") && db->getstr ("publickey", strpublickey))
            {
                dstseeddomain.napublickey.setnodepublic (strpublickey);
            }
            else
            {
                dstseeddomain.napublickey.clear ();
            }

            std::string     strsource   = db->getstrbinary ("source");
            dstseeddomain.vssource  = static_cast<validatorsource> (strsource[0]);

            inext   = db->getint ("next");
            dstseeddomain.tpnext    = ptfromseconds (inext);
            iscan   = db->getint ("scan");
            dstseeddomain.tpscan    = ptfromseconds (iscan);
            ifetch  = db->getint ("fetch");
            dstseeddomain.tpfetch   = ptfromseconds (ifetch);

            if (!db->getnull ("sha256") && db->getstr ("sha256", strsha256))
            {
                dstseeddomain.isha256.sethex (strsha256);
            }
            else
            {
                dstseeddomain.isha256.zero ();
            }

            dstseeddomain.strcomment    = db->getstrbinary ("comment");

            db->enditerrows ();
        }

        return bresult;
    }

    //--------------------------------------------------------------------------

    // persist a seeddomain.
    void setseeddomains (const seeddomain& sdsource, bool bnext)
    {
        auto db = getapp().getwalletdb ().getdb ();

        int     inext   = itoseconds (sdsource.tpnext);
        int     iscan   = itoseconds (sdsource.tpscan);
        int     ifetch  = itoseconds (sdsource.tpfetch);

        // writelog (lstrace) << str(boost::format("setseeddomains: inext=%s tpnext=%s") % inext % sdsource.tpnext);

        std::string strsql  = boost::str (boost::format ("replace into seeddomains (domain,publickey,source,next,scan,fetch,sha256,comment) values (%s, %s, %s, %d, %d, %d, '%s', %s);")
                                          % sqlescape (sdsource.strdomain)
                                          % (sdsource.napublickey.isvalid () ? sqlescape (sdsource.napublickey.humannodepublic ()) : "null")
                                          % sqlescape (std::string (1, static_cast<char> (sdsource.vssource)))
                                          % inext
                                          % iscan
                                          % ifetch
                                          % to_string (sdsource.isha256)
                                          % sqlescape (sdsource.strcomment)
                                         );

        auto sl (getapp().getwalletdb ().lock ());

        if (!db->executesql (strsql))
        {
            // xxx check result.
            writelog (lswarning, uniquenodelist) << "setseeddomains: failed.";
        }

        if (bnext && (mtpfetchnext.is_not_a_date_time () || mtpfetchnext > sdsource.tpnext))
        {
            // schedule earlier wake up.
            fetchnext ();
        }
    }


    //--------------------------------------------------------------------------

    // retrieve a seednode from db.
    bool getseednodes (rippleaddress const& nanodepublic, seednode& dstseednode)
    {
        bool        bresult;
        auto db = getapp().getwalletdb ().getdb ();

        std::string strsql  = str (boost::format ("select * from seednodes where publickey='%s';")
                                   % nanodepublic.humannodepublic ());

        auto sl (getapp().getwalletdb ().lock ());

        bresult = db->executesql (strsql) && db->startiterrows ();

        if (bresult)
        {
            std::string     strpublickey;
            std::string     strsource;
            int             inext;
            int             iscan;
            int             ifetch;
            std::string     strsha256;

            if (!db->getnull ("publickey") && db->getstr ("publickey", strpublickey))
            {
                dstseednode.napublickey.setnodepublic (strpublickey);
            }
            else
            {
                dstseednode.napublickey.clear ();
            }

            strsource   = db->getstrbinary ("source");
            dstseednode.vssource    = static_cast<validatorsource> (strsource[0]);

            inext   = db->getint ("next");
            dstseednode.tpnext  = ptfromseconds (inext);
            iscan   = db->getint ("scan");
            dstseednode.tpscan  = ptfromseconds (iscan);
            ifetch  = db->getint ("fetch");
            dstseednode.tpfetch = ptfromseconds (ifetch);

            if (!db->getnull ("sha256") && db->getstr ("sha256", strsha256))
            {
                dstseednode.isha256.sethex (strsha256);
            }
            else
            {
                dstseednode.isha256.zero ();
            }

            dstseednode.strcomment  = db->getstrbinary ("comment");

            db->enditerrows ();
        }

        return bresult;
    }

    //--------------------------------------------------------------------------

    // persist a seednode.
    // <-- bnext: true, to do fetching if needed.
    void setseednodes (const seednode& snsource, bool bnext)
    {
        auto db = getapp().getwalletdb ().getdb ();

        int     inext   = itoseconds (snsource.tpnext);
        int     iscan   = itoseconds (snsource.tpscan);
        int     ifetch  = itoseconds (snsource.tpfetch);

        // writelog (lstrace) << str(boost::format("setseednodes: inext=%s tpnext=%s") % inext % sdsource.tpnext);

        assert (snsource.napublickey.isvalid ());

        std::string strsql  = str (boost::format ("replace into seednodes (publickey,source,next,scan,fetch,sha256,comment) values ('%s', '%c', %d, %d, %d, '%s', %s);")
                                   % snsource.napublickey.humannodepublic ()
                                   % static_cast<char> (snsource.vssource)
                                   % inext
                                   % iscan
                                   % ifetch
                                   % to_string (snsource.isha256)
                                   % sqlescape (snsource.strcomment)
                                  );

        {
            auto sl (getapp().getwalletdb ().lock ());

            if (!db->executesql (strsql))
            {
                // xxx check result.
                writelog (lstrace, uniquenodelist) << "setseednodes: failed.";
            }
        }

    #if 0

        // yyy when we have a cas schedule lookups similar to this.
        if (bnext && (mtpfetchnext.is_not_a_date_time () || mtpfetchnext > snsource.tpnext))
        {
            // schedule earlier wake up.
            fetchnext ();
        }

    #else
        fetchdirty ();
    #endif
    }

    //--------------------------------------------------------------------------

    bool validatorsresponse (const boost::system::error_code& err, int istatus, std::string strresponse)
    {
        bool    breject = !err && istatus != 200;

        if (!breject)
        {
            writelog (lstrace, uniquenodelist) <<
                "fetch '" <<
                config::helpers::getvalidatorsfilename () <<
                "' complete.";

            if (!err)
            {
                nodeprocess ("network", strresponse, getconfig ().validators_site);
            }
            else
            {
                writelog (lswarning, uniquenodelist) << "error: " << err.message ();
            }
        }

        return breject;
    }

    //--------------------------------------------------------------------------

    // process a validators.txt.
    // --> strsite: source of validators
    // --> strvalidators: contents of a validators.txt
    //
    // vfalco todo can't we name this processvalidatorlist?
    //
    void nodeprocess (std::string const& strsite, std::string const& strvalidators, std::string const& strsource)
    {
        inifilesections secvalidators   = parseinifile (strvalidators, true);

        inifilesections::mapped_type*   pmtentries  = getinifilesection (secvalidators, section_validators);

        if (pmtentries)
        {
            rippleaddress   nainvalid;  // don't want a referrer on added entries.

            // yyy unspecified might be bootstrap or rpc command
            processvalidators (strsite, strsource, nainvalid, vsvalidator, pmtentries);
        }
        else
        {
            writelog (lswarning, uniquenodelist) << boost::str (boost::format ("'%s' missing [" section_validators "].")
                                                 % getconfig ().validators_base);
        }
    }
private:
    typedef ripplemutex fetchlocktype;
    typedef std::lock_guard <fetchlocktype> scopedfetchlocktype;
    fetchlocktype mfetchlock;

    typedef ripplerecursivemutex unllocktype;
    typedef std::lock_guard <unllocktype> scopedunllocktype;
    unllocktype munllock;

    // vfalco todo replace ptime with beast::time
    // misc persistent information
    boost::posix_time::ptime        mtpscoreupdated;
    boost::posix_time::ptime        mtpfetchupdated;

    // xxx make this faster, make this the contents vector unsigned char or raw public key.
    // xxx contents needs to based on score.
    hash_set<std::string>   munl;

    boost::posix_time::ptime        mtpscorenext;       // when to start scoring.
    boost::posix_time::ptime        mtpscorestart;      // time currently started scoring.
    beast::deadlinetimer m_scoretimer;                  // timer to start scoring.

    int                             mfetchactive;       // count of active fetches.

    boost::posix_time::ptime        mtpfetchnext;       // time of to start next fetch.
    beast::deadlinetimer m_fetchtimer;                  // timer to start fetching.

    std::map<rippleaddress, clusternodestatus> m_clusternodes;

    std::string node_file_name_;
    std::string node_file_path_;
};

//------------------------------------------------------------------------------

uniquenodelist::uniquenodelist (stoppable& parent)
    : stoppable ("uniquenodelist", parent)
{
}

//------------------------------------------------------------------------------

std::unique_ptr<uniquenodelist>
make_uniquenodelist (beast::stoppable& parent)
{
    return std::make_unique<uniquenodelistimp> (parent);
}

} // ripple
