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
#include <ripple/protocol/buildinfo.h>
#include <beast/unit_test/suite.h>
#include <beast/module/core/diagnostic/fatalerror.h>
#include <beast/module/core/diagnostic/semanticversion.h>

namespace ripple {

namespace buildinfo {

char const* getrawversionstring ()
{
    static char const* const rawtext =

    //--------------------------------------------------------------------------
    //
    //  the build version number (edit this for each release)
    //
        "1.2.0"
    //
    //  must follow the format described here:
    //
    //  http://semver.org/
    //
#ifdef debug
        "+debug"
#endif
    //--------------------------------------------------------------------------
    ;

    return rawtext;
}

protocolversion const&
getcurrentprotocol ()
{
    static protocolversion currentprotocol (
    //--------------------------------------------------------------------------
    //
    // the protocol version we speak and prefer (edit this if necessary)
    //
        1,  // major
        2   // minor
    //
    //--------------------------------------------------------------------------
    );

    return currentprotocol;
}

protocolversion const&
getminimumprotocol ()
{
    static protocolversion minimumprotocol (

    //--------------------------------------------------------------------------
    //
    // the oldest protocol version we will accept. (edit this if necessary)
    //
        1,  // major
        2   // minor
    //
    //--------------------------------------------------------------------------
    );

    return minimumprotocol;
}

//
//
// don't touch anything below this line
//
//------------------------------------------------------------------------------

std::string const&
getversionstring ()
{
    struct sanitychecker
    {
        sanitychecker ()
        {
            beast::semanticversion v;

            char const* const rawtext = getrawversionstring ();

            if (! v.parse (rawtext) || v.print () != rawtext)
                beast::fatalerror ("bad server version string", __file__, __line__);

            versionstring = rawtext;
        }

        std::string versionstring;
    };

    static sanitychecker const value;

    return value.versionstring;
}

std::string const& getfullversionstring ()
{
    struct prettyprinter
    {
        prettyprinter ()
        {
            fullversionstring = "moorecoind-" + getversionstring ();
#ifdef build_version
            std::string buildsignature(build_version);
            if (!buildsignature.empty())
            {
                beast::string s;
                s << " (" << buildsignature << ")";
                fullversionstring += s.tostdstring();
            }
#endif
        }

        std::string fullversionstring;
    };

    static prettyprinter const value;

    return value.fullversionstring;
}

protocolversion
make_protocol (std::uint32_t version)
{
    return protocolversion(
        static_cast<std::uint16_t> ((version >> 16) & 0xffff),
        static_cast<std::uint16_t> (version & 0xffff));
}

}

std::string
to_string (protocolversion const& p)
{
    return std::to_string (p.first) + "." + std::to_string (p.second);
}

std::uint32_t
to_packed (protocolversion const& p)
{
    return (static_cast<std::uint32_t> (p.first) << 16) + p.second;
}

} // ripple
