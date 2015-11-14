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

#ifndef ripple_protocol_stobject_h_included
#define ripple_protocol_stobject_h_included

#include <ripple/basics/countedobject.h>
#include <ripple/protocol/stamount.h>
#include <ripple/protocol/stpathset.h>
#include <ripple/protocol/stvector256.h>
#include <ripple/protocol/sotemplate.h>
#include <boost/ptr_container/ptr_vector.hpp>

namespace ripple {

class starray;

class stobject
    : public stbase
    , public countedobject <stobject>
{
public:
    static char const* getcountedobjectname () { return "stobject"; }

    stobject () : mtype (nullptr)
    {
        ;
    }

    explicit stobject (sfield::ref name)
        : stbase (name), mtype (nullptr)
    {
        ;
    }

    stobject (const sotemplate & type, sfield::ref name)
        : stbase (name)
    {
        set (type);
    }

    stobject (
        const sotemplate & type, serializeriterator & sit, sfield::ref name)
        : stbase (name)
    {
        set (sit);
        settype (type);
    }

    stobject (sfield::ref name, boost::ptr_vector<stbase>& data)
        : stbase (name), mtype (nullptr)
    {
        mdata.swap (data);
    }

    std::unique_ptr <stobject> oclone () const
    {
        return std::make_unique <stobject> (*this);
    }

    virtual ~stobject () { }

    static std::unique_ptr<stbase>
    deserialize (serializeriterator & sit, sfield::ref name);

    bool settype (const sotemplate & type);
    bool isvalidfortype ();
    bool isfieldallowed (sfield::ref);
    bool isfree () const
    {
        return mtype == nullptr;
    }

    void set (const sotemplate&);
    bool set (serializeriterator & u, int depth = 0);

    virtual serializedtypeid getstype () const override
    {
        return sti_object;
    }
    virtual bool isequivalent (const stbase & t) const override;
    virtual bool isdefault () const override
    {
        return mdata.empty ();
    }

    virtual void add (serializer & s) const override
    {
        add (s, true);    // just inner elements
    }

    void add (serializer & s, bool withsignature) const;

    // vfalco note does this return an expensive copy of an object with a
    //             dynamic buffer?
    // vfalco todo remove this function and fix the few callers.
    serializer getserializer () const
    {
        serializer s;
        add (s);
        return s;
    }

    virtual std::string getfulltext () const override;
    virtual std::string gettext () const override;

    // todo(tom): options should be an enum.
    virtual json::value getjson (int options) const override;

    int addobject (const stbase & t)
    {
        mdata.push_back (t.clone ().release ());
        return mdata.size () - 1;
    }
    int giveobject (std::unique_ptr<stbase> t)
    {
        mdata.push_back (t.release ());
        return mdata.size () - 1;
    }
    int giveobject (stbase * t)
    {
        mdata.push_back (t);
        return mdata.size () - 1;
    }
    const boost::ptr_vector<stbase>& peekdata () const
    {
        return mdata;
    }
    boost::ptr_vector<stbase>& peekdata ()
    {
        return mdata;
    }
    stbase& front ()
    {
        return mdata.front ();
    }
    const stbase& front () const
    {
        return mdata.front ();
    }
    stbase& back ()
    {
        return mdata.back ();
    }
    const stbase& back () const
    {
        return mdata.back ();
    }

    int getcount () const
    {
        return mdata.size ();
    }

    bool setflag (std::uint32_t);
    bool clearflag (std::uint32_t);
    bool isflag(std::uint32_t) const;
    std::uint32_t getflags () const;

    uint256 gethash (std::uint32_t prefix) const;
    uint256 getsigninghash (std::uint32_t prefix) const;

    const stbase& peekatindex (int offset) const
    {
        return mdata[offset];
    }
    stbase& getindex (int offset)
    {
        return mdata[offset];
    }
    const stbase* peekatpindex (int offset) const
    {
        return & (mdata[offset]);
    }
    stbase* getpindex (int offset)
    {
        return & (mdata[offset]);
    }

    int getfieldindex (sfield::ref field) const;
    sfield::ref getfieldstype (int index) const;

    const stbase& peekatfield (sfield::ref field) const;
    stbase& getfield (sfield::ref field);
    const stbase* peekatpfield (sfield::ref field) const;
    stbase* getpfield (sfield::ref field, bool createokay = false);

    // these throw if the field type doesn't match, or return default values
    // if the field is optional but not present
    std::string getfieldstring (sfield::ref field) const;
    unsigned char getfieldu8 (sfield::ref field) const;
    std::uint16_t getfieldu16 (sfield::ref field) const;
    std::uint32_t getfieldu32 (sfield::ref field) const;
    std::uint64_t getfieldu64 (sfield::ref field) const;
    uint128 getfieldh128 (sfield::ref field) const;

    uint160 getfieldh160 (sfield::ref field) const;
    uint256 getfieldh256 (sfield::ref field) const;
    rippleaddress getfieldaccount (sfield::ref field) const;
    account getfieldaccount160 (sfield::ref field) const;

    blob getfieldvl (sfield::ref field) const;
    stamount const& getfieldamount (sfield::ref field) const;
    stpathset const& getfieldpathset (sfield::ref field) const;
    const stvector256& getfieldv256 (sfield::ref field) const;
    const starray& getfieldarray (sfield::ref field) const;

    void setfieldu8 (sfield::ref field, unsigned char);
    void setfieldu16 (sfield::ref field, std::uint16_t);
    void setfieldu32 (sfield::ref field, std::uint32_t);
    void setfieldu64 (sfield::ref field, std::uint64_t);
    void setfieldh128 (sfield::ref field, uint128 const&);
    void setfieldh256 (sfield::ref field, uint256 const& );
    void setfieldvl (sfield::ref field, blob const&);
    void setfieldaccount (sfield::ref field, account const&);
    void setfieldaccount (sfield::ref field, rippleaddress const& addr)
    {
        setfieldaccount (field, addr.getaccountid ());
    }
    void setfieldamount (sfield::ref field, stamount const&);
    void setfieldpathset (sfield::ref field, stpathset const&);
    void setfieldv256 (sfield::ref field, stvector256 const& v);
    void setfieldarray (sfield::ref field, starray const& v);

    template <class tag>
    void setfieldh160 (sfield::ref field, base_uint<160, tag> const& v)
    {
        stbase* rf = getpfield (field, true);

        if (!rf)
            throw std::runtime_error ("field not found");

        if (rf->getstype () == sti_notpresent)
            rf = makefieldpresent (field);

        using bits = stbitstring<160>;
        if (auto cf = dynamic_cast<bits*> (rf))
            cf->setvalue (v);
        else
            throw std::runtime_error ("wrong field type");
    }

    stobject& peekfieldobject (sfield::ref field);

    bool isfieldpresent (sfield::ref field) const;
    stbase* makefieldpresent (sfield::ref field);
    void makefieldabsent (sfield::ref field);
    bool delfield (sfield::ref field);
    void delfield (int index);

    static std::unique_ptr <stbase>
    makedefaultobject (serializedtypeid id, sfield::ref name);

    // vfalco todo remove the 'depth' parameter
    static std::unique_ptr<stbase> makedeserializedobject (
        serializedtypeid id,
        sfield::ref name,
        serializeriterator&,
        int depth);

    static std::unique_ptr<stbase>
    makenonpresentobject (sfield::ref name)
    {
        return makedefaultobject (sti_notpresent, name);
    }

    static std::unique_ptr<stbase> makedefaultobject (sfield::ref name)
    {
        return makedefaultobject (name.fieldtype, name);
    }

    // field iterator stuff
    typedef boost::ptr_vector<stbase>::iterator iterator;
    typedef boost::ptr_vector<stbase>::const_iterator const_iterator;
    iterator begin ()
    {
        return mdata.begin ();
    }
    iterator end ()
    {
        return mdata.end ();
    }
    const_iterator begin () const
    {
        return mdata.begin ();
    }
    const_iterator end () const
    {
        return mdata.end ();
    }
    bool empty () const
    {
        return mdata.empty ();
    }

    bool hasmatchingentry (const stbase&);

    bool operator== (const stobject & o) const;
    bool operator!= (const stobject & o) const
    {
        return ! (*this == o);
    }

private:
    virtual stobject* duplicate () const override
    {
        return new stobject (*this);
    }

    // implementation for getting (most) fields that return by value.
    //
    // the remove_cv and remove_reference are necessitated by the stbitstring
    // types.  their getvalue returns by const ref.  we return those types
    // by value.
    template <typename t, typename v =
        typename std::remove_cv < typename std::remove_reference <
            decltype (std::declval <t> ().getvalue ())>::type >::type >
    v getfieldbyvalue (sfield::ref field) const
    {
        const stbase* rf = peekatpfield (field);

        if (!rf)
            throw std::runtime_error ("field not found");

        serializedtypeid id = rf->getstype ();

        if (id == sti_notpresent)
            return v (); // optional field not present

        const t* cf = dynamic_cast<const t*> (rf);

        if (!cf)
            throw std::runtime_error ("wrong field type");

        return cf->getvalue ();
    }

    // implementations for getting (most) fields that return by const reference.
    //
    // if an absent optional field is deserialized we don't have anything
    // obvious to return.  so we insist on having the call provide an
    // 'empty' value we return in that circumstance.
    template <typename t, typename v>
    v const& getfieldbyconstref (sfield::ref field, v const& empty) const
    {
        const stbase* rf = peekatpfield (field);

        if (!rf)
            throw std::runtime_error ("field not found");

        serializedtypeid id = rf->getstype ();

        if (id == sti_notpresent)
            return empty; // optional field not present

        const t* cf = dynamic_cast<const t*> (rf);

        if (!cf)
            throw std::runtime_error ("wrong field type");

        return *cf;
    }

    // implementation for setting most fields with a setvalue() method.
    template <typename t, typename v>
    void setfieldusingsetvalue (sfield::ref field, v value)
    {
        stbase* rf = getpfield (field, true);

        if (!rf)
            throw std::runtime_error ("field not found");

        if (rf->getstype () == sti_notpresent)
            rf = makefieldpresent (field);

        t* cf = dynamic_cast<t*> (rf);

        if (!cf)
            throw std::runtime_error ("wrong field type");

        cf->setvalue (value);
    }

    // implementation for setting fields using assignment
    template <typename t>
    void setfieldusingassignment (sfield::ref field, t const& value)
    {
        stbase* rf = getpfield (field, true);

        if (!rf)
            throw std::runtime_error ("field not found");

        if (rf->getstype () == sti_notpresent)
            rf = makefieldpresent (field);

        t* cf = dynamic_cast<t*> (rf);

        if (!cf)
            throw std::runtime_error ("wrong field type");

        (*cf) = value;
    }

private:
    boost::ptr_vector<stbase>   mdata;
    const sotemplate*                   mtype;
};

} // ripple

#endif
