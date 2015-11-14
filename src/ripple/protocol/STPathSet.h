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

#ifndef ripple_protocol_stpathelement_h_included
#define ripple_protocol_stpathelement_h_included

#include <ripple/json/json_value.h>
#include <ripple/protocol/sfield.h>
#include <ripple/protocol/stbase.h>
#include <ripple/protocol/uinttypes.h>
#include <cstddef>

namespace ripple {

// vfalco why isn't this derived from stbase?
class stpathelement
{
public:
    enum type
    {
        typenone        = 0x00,
        typeaccount     = 0x01, // rippling through an account (vs taking an offer).
        typecurrency    = 0x10, // currency follows.
        typeissuer      = 0x20, // issuer follows.
        typeboundary    = 0xff, // boundary between alternate paths.
        typeall = typeaccount | typecurrency | typeissuer,
                                // combination of all types.
    };

private:
    static
    std::size_t
    get_hash (stpathelement const& element);

public:
    stpathelement (
        account const& account, currency const& currency,
        account const& issuer, bool forcecurrency = false)
        : mtype (typenone), maccountid (account), mcurrencyid (currency)
		, missuerid(issuer), is_offer_(isnative(maccountid))
    {
        if (!is_offer_)
            mtype |= typeaccount;

        if (forcecurrency || !isnative(currency))
            mtype |= typecurrency;

        if (!isnative(issuer))
            mtype |= typeissuer;

        hash_value_ = get_hash (*this);
    }

    stpathelement (
        unsigned int utype, account const& account, currency const& currency,
        account const& issuer)
        : mtype (utype), maccountid (account), mcurrencyid (currency)
		, missuerid(issuer), is_offer_(isnative(maccountid))
    {
        hash_value_ = get_hash (*this);
    }

    stpathelement ()
        : mtype (typenone), is_offer_ (true)
    {
        hash_value_ = get_hash (*this);
    }

    int getnodetype () const
    {
        return mtype;
    }
    bool isoffer () const
    {
        return is_offer_;
    }
    bool isaccount () const
    {
        return !isoffer ();
    }

    // nodes are either an account id or a offer prefix. offer prefixs denote a
    // class of offers.
    account const& getaccountid () const
    {
        return maccountid;
    }
    currency const& getcurrency () const
    {
        return mcurrencyid;
    }
    account const& getissuerid () const
    {
        return missuerid;
    }

    bool operator== (const stpathelement& t) const
    {
        return (mtype & typeaccount) == (t.mtype & typeaccount) &&
            hash_value_ == t.hash_value_ &&
            maccountid == t.maccountid &&
            mcurrencyid == t.mcurrencyid &&
            missuerid == t.missuerid;
    }

private:
    unsigned int mtype;
    account maccountid;
    currency mcurrencyid;
    account missuerid;

    bool is_offer_;
    std::size_t hash_value_;
};

class stpath
{
public:
    stpath () = default;

    stpath (std::vector<stpathelement> const& p)
        : mpath (p)
    { }

    std::vector<stpathelement>::size_type
    size () const
    {
        return mpath.size ();
    }

    bool empty() const
    {
        return mpath.empty ();
    }

    void
    push_back (stpathelement const& e)
    {
        mpath.push_back (e);
    }

    template <typename ...args>
    void
    emplace_back (args&&... args)
    {
        mpath.emplace_back (std::forward<args> (args)...);
    }

    bool hasseen (account const& account, currency const& currency,
                  account const& issuer) const;
    json::value getjson (int) const;

    std::vector<stpathelement>::const_iterator
    begin () const
    {
        return mpath.begin ();
    }

    std::vector<stpathelement>::const_iterator
    end () const
    {
        return mpath.end ();
    }

    bool operator== (stpath const& t) const
    {
        return mpath == t.mpath;
    }

    std::vector<stpathelement>::const_reference
    back () const
    {
        return mpath.back ();
    }

    std::vector<stpathelement>::const_reference
    front () const
    {
        return mpath.front ();
    }

private:
    std::vector<stpathelement> mpath;
};

//------------------------------------------------------------------------------

// a set of zero or more payment paths
class stpathset : public stbase
{
public:
    stpathset () = default;

    stpathset (sfield::ref n)
        : stbase (n)
    { }

    static
    std::unique_ptr<stbase>
    deserialize (serializeriterator& sit, sfield::ref name)
    {
        return std::unique_ptr<stbase> (construct (sit, name));
    }

    void add (serializer& s) const;
    virtual json::value getjson (int) const;

    serializedtypeid getstype () const
    {
        return sti_pathset;
    }
    std::vector<stpath>::size_type
    size () const
    {
        return value.size ();
    }

    bool empty () const
    {
        return value.empty ();
    }

    void push_back (stpath const& e)
    {
        value.push_back (e);
    }

    bool assembleadd(stpath const& base, stpathelement const& tail)
    { // assemble base+tail and add it to the set if it's not a duplicate
        value.push_back (base);

        std::vector<stpath>::reverse_iterator it = value.rbegin ();

        stpath& newpath = *it;
        newpath.push_back (tail);

        while (++it != value.rend ())
        {
            if (*it == newpath)
            {
                value.pop_back ();
                return false;
            }
        }
        return true;
    }

    virtual bool isequivalent (const stbase& t) const;
    virtual bool isdefault () const
    {
        return value.empty ();
    }

    std::vector<stpath>::const_reference
    operator[] (std::vector<stpath>::size_type n) const
    {
        return value[n];
    }

    std::vector<stpath>::const_iterator begin () const
    {
        return value.begin ();
    }

    std::vector<stpath>::const_iterator end () const
    {
        return value.end ();
    }

private:
    std::vector<stpath> value;

    stpathset (sfield::ref n, const std::vector<stpath>& v)
        : stbase (n), value (v)
    { }

    stpathset* duplicate () const
    {
        return new stpathset (*this);
    }

    static
    stpathset*
    construct (serializeriterator&, sfield::ref);
};

} // ripple

#endif
