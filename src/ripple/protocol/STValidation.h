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

#ifndef ripple_protocol_stvalidation_h_included
#define ripple_protocol_stvalidation_h_included

#include <ripple/protocol/rippleaddress.h>
#include <ripple/protocol/stobject.h>
#include <cstdint>
#include <memory>

namespace ripple {

// validation flags
const std::uint32_t vffullycanonicalsig    = 0x80000000; // signature is fully canonical

class stvalidation
    : public stobject
    , public countedobject <stvalidation>
{
public:
    static char const* getcountedobjectname () { return "stvalidation"; }

    typedef std::shared_ptr<stvalidation>         pointer;
    typedef const std::shared_ptr<stvalidation>&  ref;

    enum
    {
        kfullflag = 0x1
    };

    // these throw if the object is not valid
    stvalidation (serializeriterator & sit, bool checksignature = true);

    // does not sign the validation
    stvalidation (uint256 const& ledgerhash, std::uint32_t signtime,
                          const rippleaddress & rapub, bool isfull);

    uint256         getledgerhash ()     const;
    std::uint32_t   getsigntime ()       const;
    std::uint32_t   getflags ()          const;
    rippleaddress   getsignerpublic ()   const;
    nodeid          getnodeid ()         const
    {
        return mnodeid;
    }
    bool            isvalid ()           const;
    bool            isfull ()            const;
    bool            istrusted ()         const
    {
        return mtrusted;
    }
    uint256         getsigninghash ()    const;
    bool            isvalid (uint256 const& ) const;

    void                        settrusted ()
    {
        mtrusted = true;
    }
    blob    getsigned ()                 const;
    blob    getsignature ()              const;
    void sign (uint256 & signinghash, const rippleaddress & raprivate);
    void sign (const rippleaddress & raprivate);

    // the validation this replaced
    uint256 const& getprevioushash ()
    {
        return mprevioushash;
    }
    bool isprevioushash (uint256 const& h) const
    {
        return mprevioushash == h;
    }
    void setprevioushash (uint256 const& h)
    {
        mprevioushash = h;
    }

private:
    static sotemplate const& getformat ();

    void setnode ();

    uint256 mprevioushash;
    nodeid mnodeid;
    bool mtrusted;
};

} // ripple

#endif
