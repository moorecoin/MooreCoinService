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
#include <ripple/basics/log.h>
#include <ripple/json/json_reader.h>
#include <ripple/json/to_string.h>
#include <ripple/protocol/stbase.h>
#include <ripple/protocol/staccount.h>
#include <ripple/protocol/starray.h>
#include <ripple/protocol/stobject.h>
#include <ripple/protocol/stparsedjson.h>
#include <beast/module/core/text/lexicalcast.h>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

std::unique_ptr<stbase>
stobject::makedefaultobject (serializedtypeid id, sfield::ref name)
{
    assert ((id == sti_notpresent) || (id == name.fieldtype));

    switch (id)
    {
    case sti_notpresent:
        return std::make_unique <stbase> (name);

    case sti_uint8:
        return std::make_unique <stuint8> (name);

    case sti_uint16:
        return std::make_unique <stuint16> (name);

    case sti_uint32:
        return std::make_unique <stuint32> (name);

    case sti_uint64:
        return std::make_unique <stuint64> (name);

    case sti_amount:
        return std::make_unique <stamount> (name);

    case sti_hash128:
        return std::make_unique <sthash128> (name);

    case sti_hash160:
        return std::make_unique <sthash160> (name);

    case sti_hash256:
        return std::make_unique <sthash256> (name);

    case sti_vector256:
        return std::make_unique <stvector256> (name);

    case sti_vl:
        return std::make_unique <stblob> (name);

    case sti_account:
        return std::make_unique <staccount> (name);

    case sti_pathset:
        return std::make_unique <stpathset> (name);

    case sti_object:
        return std::make_unique <stobject> (name);

    case sti_array:
        return std::make_unique <starray> (name);

    default:
        writelog (lsfatal, stobject) <<
            "object type: " << beast::lexicalcast <std::string> (id);
        assert (false);
        throw std::runtime_error ("unknown object type");
    }
}

// vfalco todo remove the 'depth' parameter
std::unique_ptr<stbase>
stobject::makedeserializedobject (serializedtypeid id, sfield::ref name,
        serializeriterator& sit, int depth)
{
    switch (id)
    {
    case sti_notpresent:
        return stbase::deserialize (name);

    case sti_uint8:
        return stuint8::deserialize (sit, name);

    case sti_uint16:
        return stuint16::deserialize (sit, name);

    case sti_uint32:
        return stuint32::deserialize (sit, name);

    case sti_uint64:
        return stuint64::deserialize (sit, name);

    case sti_amount:
        return stamount::deserialize (sit, name);

    case sti_hash128:
        return sthash128::deserialize (sit, name);

    case sti_hash160:
        return sthash160::deserialize (sit, name);

    case sti_hash256:
        return sthash256::deserialize (sit, name);

    case sti_vector256:
        return stvector256::deserialize (sit, name);

    case sti_vl:
        return stblob::deserialize (sit, name);

    case sti_account:
        return staccount::deserialize (sit, name);

    case sti_pathset:
        return stpathset::deserialize (sit, name);

    case sti_array:
        return starray::deserialize (sit, name);

    case sti_object:
        return stobject::deserialize (sit, name);

    default:
        throw std::runtime_error ("unknown object type");
    }
}

void stobject::set (const sotemplate& type)
{
    mdata.clear ();
    mtype = &type;

    for (sotemplate::value_type const& elem : type.peek ())
    {
        if (elem->flags != soe_required)
            giveobject (makenonpresentobject (elem->e_field));
        else
            giveobject (makedefaultobject (elem->e_field));
    }
}

bool stobject::settype (const sotemplate& type)
{
    boost::ptr_vector<stbase> newdata (type.peek ().size ());
    bool valid = true;

    mtype = &type;

    stbase** array = mdata.c_array();
    std::size_t count = mdata.size ();

    for (auto const& elem : type.peek ())
    {
        // loop through all the fields in the template
        bool match = false;

        for (std::size_t i = 0; i < count; ++i)
            if ((array[i] != nullptr) &&
                (array[i]->getfname () == elem->e_field))
            {
                // matching entry in the object, move to new vector
                match = true;

                if ((elem->flags == soe_default) && array[i]->isdefault ())
                {
                    writelog (lswarning, stobject) <<
                        "settype( " << getfname ().getname () <<
                        ") invalid default " << elem->e_field.fieldname;
                    valid = false;
                }

                newdata.push_back (array[i]);
                array[i] = nullptr;
                break;
            }

        if (!match)
        {
            // no match found in the object for an entry in the template
            if (elem->flags == soe_required)
            {
                writelog (lswarning, stobject) <<
                    "settype( " << getfname ().getname () <<
                    ") invalid missing " << elem->e_field.fieldname;
                valid = false;
            }

            // make a default object
            newdata.push_back (makenonpresentobject (elem->e_field).release ());
        }
    }

    for (std::size_t i = 0; i < count; ++i)
    {
        // anything left over in the object must be discardable
        if ((array[i] != nullptr) && !array[i]->getfname ().isdiscardable ())
        {
            writelog (lswarning, stobject) <<
                "settype( " << getfname ().getname () <<
                ") invalid leftover " << array[i]->getfname ().getname ();
            valid = false;
        }
    }

    // swap the template matching data in for the old data,
    // freeing any leftover junk
    mdata.swap (newdata);

    return valid;
}

bool stobject::isvalidfortype ()
{
    boost::ptr_vector<stbase>::iterator it = mdata.begin ();

    for (sotemplate::value_type const& elem : mtype->peek ())
    {
        if (it == mdata.end ())
            return false;

        if (elem->e_field != it->getfname ())
            return false;

        ++it;
    }

    return true;
}

bool stobject::isfieldallowed (sfield::ref field)
{
    if (mtype == nullptr)
        return true;

    return mtype->getindex (field) != -1;
}

// return true = terminated with end-of-object
bool stobject::set (serializeriterator& sit, int depth)
{
    bool reachedendofobject = false;

    // empty the destination buffer
    //
    mdata.clear ();

    // consume data in the pipe until we run out or reach the end
    //
    while (!reachedendofobject && !sit.empty ())
    {
        int type;
        int field;

        // get the metadata for the next field
        //
        sit.getfieldid (type, field);

        reachedendofobject = (type == sti_object) && (field == 1);

        if ((type == sti_array) && (field == 1))
        {
            writelog (lswarning, stobject) <<
                "encountered object with end of array marker";
            throw std::runtime_error ("illegal terminator in object");
        }

        if (!reachedendofobject)
        {
            // figure out the field
            //
            sfield::ref fn = sfield::getfield (type, field);

            if (fn.isinvalid ())
            {
                writelog (lswarning, stobject) <<
                    "unknown field: field_type=" << type <<
                    ", field_name=" << field;
                throw std::runtime_error ("unknown field");
            }

            // unflatten the field
            //
            giveobject (
                makedeserializedobject (fn.fieldtype, fn, sit, depth + 1));
        }
    }

    return reachedendofobject;
}


std::unique_ptr<stbase>
stobject::deserialize (serializeriterator& sit, sfield::ref name)
{
    std::unique_ptr <stobject> object (std::make_unique <stobject> (name));
    object->set (sit, 1);
    return std::move (object);
}

bool stobject::hasmatchingentry (const stbase& t)
{
    const stbase* o = peekatpfield (t.getfname ());

    if (!o)
        return false;

    return t == *o;
}

std::string stobject::getfulltext () const
{
    std::string ret;
    bool first = true;

    if (fname->hasname ())
    {
        ret = fname->getname ();
        ret += " = {";
    }
    else ret = "{";

    for (stbase const& elem : mdata)
    {
        if (elem.getstype () != sti_notpresent)
        {
            if (!first)
                ret += ", ";
            else
                first = false;

            ret += elem.getfulltext ();
        }
    }

    ret += "}";
    return ret;
}

void stobject::add (serializer& s, bool withsigningfields) const
{
    std::map<int, const stbase*> fields;

    for (stbase const& elem : mdata)
    {
        // pick out the fields and sort them
        if ((elem.getstype () != sti_notpresent) &&
            elem.getfname ().shouldinclude (withsigningfields))
        {
            fields.insert (std::make_pair (elem.getfname ().fieldcode, &elem));
        }
    }

    typedef std::map<int, const stbase*>::value_type field_iterator;
    for (auto const& mapentry : fields)
    {
        // insert them in sorted order
        const stbase* field = mapentry.second;

        // when we serialize an object inside another object,
        // the type associated by rule with this field name
        // must be object, or the object cannot be deserialized
        assert ((field->getstype() != sti_object) ||
            (field->getfname().fieldtype == sti_object));

        field->addfieldid (s);
        field->add (s);

        if (dynamic_cast<const starray*> (field) != nullptr)
            s.addfieldid (sti_array, 1);
        else if (dynamic_cast<const stobject*> (field) != nullptr)
            s.addfieldid (sti_object, 1);
    }
}

std::string stobject::gettext () const
{
    std::string ret = "{";
    bool first = false;
    for (stbase const& elem : mdata)
    {
        if (!first)
        {
            ret += ", ";
            first = false;
        }

        ret += elem.gettext ();
    }
    ret += "}";
    return ret;
}

bool stobject::isequivalent (const stbase& t) const
{
    const stobject* v = dynamic_cast<const stobject*> (&t);

    if (!v)
    {
        writelog (lsdebug, stobject) <<
            "notequiv " << getfulltext() << " not object";
        return false;
    }

    typedef boost::ptr_vector<stbase>::const_iterator const_iter;
    const_iter it1 = mdata.begin (), end1 = mdata.end ();
    const_iter it2 = v->mdata.begin (), end2 = v->mdata.end ();

    while ((it1 != end1) && (it2 != end2))
    {
        if ((it1->getstype () != it2->getstype ()) || !it1->isequivalent (*it2))
        {
            if (it1->getstype () != it2->getstype ())
            {
                writelog (lsdebug, stobject) << "notequiv type " <<
                    it1->getfulltext() << " != " <<  it2->getfulltext();
            }
            else
            {
                writelog (lsdebug, stobject) << "notequiv " <<
                     it1->getfulltext() << " != " <<  it2->getfulltext();
            }
            return false;
        }

        ++it1;
        ++it2;
    }

    return (it1 == end1) && (it2 == end2);
}

uint256 stobject::gethash (std::uint32_t prefix) const
{
    serializer s;
    s.add32 (prefix);
    add (s, true);
    return s.getsha512half ();
}

uint256 stobject::getsigninghash (std::uint32_t prefix) const
{
    serializer s;
    s.add32 (prefix);
    add (s, false);
    return s.getsha512half ();
}

int stobject::getfieldindex (sfield::ref field) const
{
    if (mtype != nullptr)
        return mtype->getindex (field);

    int i = 0;
    for (stbase const& elem : mdata)
    {
        if (elem.getfname () == field)
            return i;

        ++i;
    }
    return -1;
}

const stbase& stobject::peekatfield (sfield::ref field) const
{
    int index = getfieldindex (field);

    if (index == -1)
        throw std::runtime_error ("field not found");

    return peekatindex (index);
}

stbase& stobject::getfield (sfield::ref field)
{
    int index = getfieldindex (field);

    if (index == -1)
        throw std::runtime_error ("field not found");

    return getindex (index);
}

sfield::ref stobject::getfieldstype (int index) const
{
    return mdata[index].getfname ();
}

const stbase* stobject::peekatpfield (sfield::ref field) const
{
    int index = getfieldindex (field);

    if (index == -1)
        return nullptr;

    return peekatpindex (index);
}

stbase* stobject::getpfield (sfield::ref field, bool createokay)
{
    int index = getfieldindex (field);

    if (index == -1)
    {
        if (createokay && isfree ())
            return getpindex (giveobject (makedefaultobject (field)));

        return nullptr;
    }

    return getpindex (index);
}

bool stobject::isfieldpresent (sfield::ref field) const
{
    int index = getfieldindex (field);

    if (index == -1)
        return false;

    return peekatindex (index).getstype () != sti_notpresent;
}

stobject& stobject::peekfieldobject (sfield::ref field)
{
    stbase* rf = getpfield (field, true);

    if (!rf)
        throw std::runtime_error ("field not found");

    if (rf->getstype () == sti_notpresent)
        rf = makefieldpresent (field);

    stobject* cf = dynamic_cast<stobject*> (rf);

    if (!cf)
        throw std::runtime_error ("wrong field type");

    return *cf;
}

bool stobject::setflag (std::uint32_t f)
{
    stuint32* t = dynamic_cast<stuint32*> (getpfield (sfflags, true));

    if (!t)
        return false;

    t->setvalue (t->getvalue () | f);
    return true;
}

bool stobject::clearflag (std::uint32_t f)
{
    stuint32* t = dynamic_cast<stuint32*> (getpfield (sfflags));

    if (!t)
        return false;

    t->setvalue (t->getvalue () & ~f);
    return true;
}

bool stobject::isflag (std::uint32_t f) const
{
    return (getflags () & f) == f;
}

std::uint32_t stobject::getflags (void) const
{
    const stuint32* t = dynamic_cast<const stuint32*> (peekatpfield (sfflags));

    if (!t)
        return 0;

    return t->getvalue ();
}

stbase* stobject::makefieldpresent (sfield::ref field)
{
    int index = getfieldindex (field);

    if (index == -1)
    {
        if (!isfree ())
            throw std::runtime_error ("field not found");

        return getpindex (giveobject (makenonpresentobject (field)));
    }

    stbase* f = getpindex (index);

    if (f->getstype () != sti_notpresent)
        return f;

    mdata.replace (index, makedefaultobject (f->getfname ()).release ());
    return getpindex (index);
}

void stobject::makefieldabsent (sfield::ref field)
{
    int index = getfieldindex (field);

    if (index == -1)
        throw std::runtime_error ("field not found");

    const stbase& f = peekatindex (index);

    if (f.getstype () == sti_notpresent)
        return;

    mdata.replace (index, makenonpresentobject (f.getfname ()).release ());
}

bool stobject::delfield (sfield::ref field)
{
    int index = getfieldindex (field);

    if (index == -1)
        return false;

    delfield (index);
    return true;
}

void stobject::delfield (int index)
{
    mdata.erase (mdata.begin () + index);
}

std::string stobject::getfieldstring (sfield::ref field) const
{
    const stbase* rf = peekatpfield (field);

    if (!rf) throw std::runtime_error ("field not found");

    return rf->gettext ();
}

unsigned char stobject::getfieldu8 (sfield::ref field) const
{
    return getfieldbyvalue <stuint8> (field);
}

std::uint16_t stobject::getfieldu16 (sfield::ref field) const
{
    return getfieldbyvalue <stuint16> (field);
}

std::uint32_t stobject::getfieldu32 (sfield::ref field) const
{
    return getfieldbyvalue <stuint32> (field);
}

std::uint64_t stobject::getfieldu64 (sfield::ref field) const
{
    return getfieldbyvalue <stuint64> (field);
}

uint128 stobject::getfieldh128 (sfield::ref field) const
{
    return getfieldbyvalue <sthash128> (field);
}

uint160 stobject::getfieldh160 (sfield::ref field) const
{
    return getfieldbyvalue <sthash160> (field);
}

uint256 stobject::getfieldh256 (sfield::ref field) const
{
    return getfieldbyvalue <sthash256> (field);
}

rippleaddress stobject::getfieldaccount (sfield::ref field) const
{
    const stbase* rf = peekatpfield (field);

    if (!rf)
        throw std::runtime_error ("field not found");

    serializedtypeid id = rf->getstype ();

    if (id == sti_notpresent) return rippleaddress ();

    const staccount* cf = dynamic_cast<const staccount*> (rf);

    if (!cf)
        throw std::runtime_error ("wrong field type");

    return cf->getvaluenca ();
}

account stobject::getfieldaccount160 (sfield::ref field) const
{
    auto rf = peekatpfield (field);
    if (!rf)
        throw std::runtime_error ("field not found");

    account account;
    if (rf->getstype () != sti_notpresent)
    {
        const staccount* cf = dynamic_cast<const staccount*> (rf);

        if (!cf)
            throw std::runtime_error ("wrong field type");

        cf->getvalueh160 (account);
    }

    return account;
}

blob stobject::getfieldvl (sfield::ref field) const
{
    return getfieldbyvalue <stblob> (field);
}

stamount const& stobject::getfieldamount (sfield::ref field) const
{
    static stamount const empty;
    return getfieldbyconstref <stamount> (field, empty);
}

const starray& stobject::getfieldarray (sfield::ref field) const
{
    static starray const empty;
    return getfieldbyconstref <starray> (field, empty);
}

stpathset const& stobject::getfieldpathset (sfield::ref field) const
{
    static stpathset const empty{};
    return getfieldbyconstref <stpathset> (field, empty);
}

const stvector256& stobject::getfieldv256 (sfield::ref field) const
{
    static stvector256 const empty{};
    return getfieldbyconstref <stvector256> (field, empty);
}

void stobject::setfieldu8 (sfield::ref field, unsigned char v)
{
    setfieldusingsetvalue <stuint8> (field, v);
}

void stobject::setfieldu16 (sfield::ref field, std::uint16_t v)
{
    setfieldusingsetvalue <stuint16> (field, v);
}

void stobject::setfieldu32 (sfield::ref field, std::uint32_t v)
{
    setfieldusingsetvalue <stuint32> (field, v);
}

void stobject::setfieldu64 (sfield::ref field, std::uint64_t v)
{
    setfieldusingsetvalue <stuint64> (field, v);
}

void stobject::setfieldh128 (sfield::ref field, uint128 const& v)
{
    setfieldusingsetvalue <sthash128> (field, v);
}

void stobject::setfieldh256 (sfield::ref field, uint256 const& v)
{
    setfieldusingsetvalue <sthash256> (field, v);
}

void stobject::setfieldv256 (sfield::ref field, stvector256 const& v)
{
    setfieldusingsetvalue <stvector256> (field, v);
}

void stobject::setfieldaccount (sfield::ref field, account const& v)
{
    stbase* rf = getpfield (field, true);

    if (!rf)
        throw std::runtime_error ("field not found");

    if (rf->getstype () == sti_notpresent)
        rf = makefieldpresent (field);

    staccount* cf = dynamic_cast<staccount*> (rf);

    if (!cf)
        throw std::runtime_error ("wrong field type");

    cf->setvalueh160 (v);
}

void stobject::setfieldvl (sfield::ref field, blob const& v)
{
    setfieldusingsetvalue <stblob> (field, v);
}

void stobject::setfieldamount (sfield::ref field, stamount const& v)
{
    setfieldusingassignment (field, v);
}

void stobject::setfieldpathset (sfield::ref field, stpathset const& v)
{
    setfieldusingassignment (field, v);
}

void stobject::setfieldarray (sfield::ref field, starray const& v)
{
    setfieldusingassignment (field, v);
}

json::value stobject::getjson (int options) const
{
    json::value ret (json::objectvalue);

    // todo(tom): this variable is never changed...?
    int index = 1;
    for (auto const& it: mdata)
    {
        if (it.getstype () != sti_notpresent)
        {
            auto const& n = it.getfname ();
            auto key = n.hasname () ? std::string(n.getjsonname ()) :
                    std::to_string (index);
            ret[key] = it.getjson (options);
        }
    }
    return ret;
}

bool stobject::operator== (const stobject& obj) const
{
    // this is not particularly efficient, and only compares data elements
    // with binary representations
    int matches = 0;
    for (stbase const& t1 : mdata)
    {
        if ((t1.getstype () != sti_notpresent) && t1.getfname ().isbinary ())
        {
            // each present field must have a matching field
            bool match = false;
            for (stbase const& t2 : obj.mdata)
            {
                if (t1.getfname () == t2.getfname ())
                {
                    if (t2 != t1)
                        return false;

                    match = true;
                    ++matches;
                    break;
                }
            }

            if (!match)
            {
                writelog (lstrace, stobject) <<
                    "stobject::operator==: no match for " <<
                    t1.getfname ().getname ();
                return false;
            }
        }
    }

    int fields = 0;
    for (stbase const& t2 : obj.mdata)
    {
        if ((t2.getstype () != sti_notpresent) && t2.getfname ().isbinary ())
            ++fields;
    }

    if (fields != matches)
    {
        writelog (lstrace, stobject) << "stobject::operator==: " <<
            fields << " fields, " << matches << " matches";
        return false;
    }

    return true;
}

} // ripple
