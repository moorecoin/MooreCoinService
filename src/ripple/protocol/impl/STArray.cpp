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
#include <ripple/protocol/stbase.h>
#include <ripple/protocol/starray.h>

namespace ripple {

std::unique_ptr<stbase>
starray::deserialize (serializeriterator& sit, sfield::ref field)
{
    std::unique_ptr <starray> ret (std::make_unique <starray> (field));
    vector& value (ret->getvalue ());

    while (!sit.empty ())
    {
        int type, field;
        sit.getfieldid (type, field);

        if ((type == sti_array) && (field == 1))
            break;

        if ((type == sti_object) && (field == 1))
        {
            writelog (lswarning, stobject) <<
                "encountered array with end of object marker";
            throw std::runtime_error ("illegal terminator in array");
        }

        sfield::ref fn = sfield::getfield (type, field);

        if (fn.isinvalid ())
        {
            writelog (lstrace, stobject) <<
                "unknown field: " << type << "/" << field;
            throw std::runtime_error ("unknown field");
        }

        if (fn.fieldtype != sti_object)
        {
            writelog (lstrace, stobject) << "array contains non-object";
            throw std::runtime_error ("non-object in array");
        }

        value.push_back (new stobject (fn));
        value.rbegin ()->set (sit, 1);
    }
    return std::move (ret);
}

std::string starray::getfulltext () const
{
    std::string r = "[";

    bool first = true;
    for (stobject const& o : value)
    {
        if (!first)
            r += ",";

        r += o.getfulltext ();
        first = false;
    }

    r += "]";
    return r;
}

std::string starray::gettext () const
{
    std::string r = "[";

    bool first = true;
    for (stobject const& o : value)
    {
        if (!first)
            r += ",";

        r += o.gettext ();
        first = false;
    }

    r += "]";
    return r;
}

json::value starray::getjson (int p) const
{
    json::value v = json::arrayvalue;
    int index = 1;
    for (auto const& object: value)
    {
        if (object.getstype () != sti_notpresent)
        {
            json::value inner = json::objectvalue;
            auto const& fname = object.getfname ();
            auto k = fname.hasname () ? fname.fieldname : std::to_string(index);
            inner[k] = object.getjson (p);
            v.append (inner);
            index++;
        }
    }
    return v;
}

void starray::add (serializer& s) const
{
    for (stobject const& object : value)
    {
        object.addfieldid (s);
        object.add (s);
        s.addfieldid (sti_object, 1);
    }
}

bool starray::isequivalent (const stbase& t) const
{
    const starray* v = dynamic_cast<const starray*> (&t);

    if (!v)
    {
        writelog (lsdebug, stobject) <<
            "notequiv " << getfulltext() << " not array";
        return false;
    }

    return value == v->value;
}

void starray::sort (bool (*compare) (const stobject&, const stobject&))
{
    value.sort (compare);
}

} // ripple
