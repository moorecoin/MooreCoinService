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
#include <ripple/basics/stringutilities.h>
#include <ripple/protocol/stinteger.h>
#include <ripple/protocol/errorcodes.h>
#include <ripple/protocol/ledgerformats.h>
#include <ripple/protocol/staccount.h>
#include <ripple/protocol/stamount.h>
#include <ripple/protocol/starray.h>
#include <ripple/protocol/stbitstring.h>
#include <ripple/protocol/stblob.h>
#include <ripple/protocol/stvector256.h>
#include <ripple/protocol/stparsedjson.h>
#include <ripple/protocol/stpathset.h>
#include <ripple/protocol/txformats.h>
#include <beast/module/core/text/lexicalcast.h>
#include <cassert>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

namespace stparsedjsondetail
{

template <typename t, typename u>
static t range_check_cast (u value, t minimum, t maximum)
{
    if ((value < minimum) || (value > maximum))
        throw std::runtime_error ("value out of range");

    return static_cast<t> (value);
}

static std::string make_name (std::string const& object,
    std::string const& field)
{
    if (field.empty ())
        return object;

    return object + "." + field;
}

static json::value not_an_object (std::string const& object,
    std::string const& field)
{
    return rpc::make_error (rpcinvalid_params,
        "field '" + make_name (object, field) + "' is not a json object.");
}

static json::value not_an_object (std::string const& object)
{
    return not_an_object (object, "");
}

static json::value not_an_array (std::string const& object)
{
    return rpc::make_error (rpcinvalid_params,
        "field '" + object + "' is not a json array.");
}

static json::value unknown_field (std::string const& object,
    std::string const& field)
{
    return rpc::make_error (rpcinvalid_params,
        "field '" + make_name (object, field) + "' is unknown.");
}

static json::value out_of_range (std::string const& object,
    std::string const& field)
{
    return rpc::make_error (rpcinvalid_params,
        "field '" + make_name (object, field) + "' is out of range.");
}

static json::value bad_type (std::string const& object,
    std::string const& field)
{
    return rpc::make_error (rpcinvalid_params,
        "field '" + make_name (object, field) + "' has bad type.");
}

static json::value invalid_data (std::string const& object,
    std::string const& field)
{
    return rpc::make_error (rpcinvalid_params,
        "field '" + make_name (object, field) + "' has invalid data.");
}

static json::value invalid_data (std::string const& object)
{
    return invalid_data (object, "");
}

static json::value array_expected (std::string const& object,
    std::string const& field)
{
    return rpc::make_error (rpcinvalid_params,
        "field '" + make_name (object, field) + "' must be a json array.");
}

static json::value string_expected (std::string const& object,
    std::string const& field)
{
    return rpc::make_error (rpcinvalid_params,
        "field '" + make_name (object, field) + "' must be a string.");
}

static json::value too_deep (std::string const& object)
{
    return rpc::make_error (rpcinvalid_params,
        "field '" + object + "' exceeds nesting depth limit.");
}

static json::value singleton_expected (std::string const& object,
    unsigned int index)
{
    return rpc::make_error (rpcinvalid_params,
        "field '" + object + "[" + std::to_string (index) +
            "]' must be an object with a single key/object value.");
}


// this function is used by parseobject to parse any json type that doesn't
// recurse.  everything represented here is a leaf-type.
static std::unique_ptr <stbase> parseleaf (
    std::string const& json_name,
    std::string const& fieldname,
    sfield::ptr name,
    json::value const& value,
    json::value& error)
{
    std::unique_ptr <stbase> ret;

    sfield::ref field = sfield::getfield (fieldname);

    if (field == sfinvalid)
    {
        error = unknown_field (json_name, fieldname);
        return ret;
    }

    switch (field.fieldtype)
    {
    case sti_uint8:
        try
        {
            if (value.isstring ())
            {
                // vfalco todo wtf?
            }
            else if (value.isint ())
            {
                if (value.asint () < 0 || value.asint () > 255)
                {
                    error = out_of_range (json_name, fieldname);
                    return ret;
                }

                ret = std::make_unique <stuint8> (field,
                    range_check_cast <unsigned char> (
                        value.asint (), 0, 255));
            }
            else if (value.isuint ())
            {
                if (value.asuint () > 255)
                {
                    error = out_of_range (json_name, fieldname);
                    return ret;
                }

                ret = std::make_unique <stuint8> (field,
                    range_check_cast <unsigned char> (
                        value.asuint (), 0, 255));
            }
            else
            {
                error = bad_type (json_name, fieldname);
                return ret;
            }
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }
        break;

    case sti_uint16:
        try
        {
            if (value.isstring ())
            {
                std::string const strvalue = value.asstring ();

                if (! strvalue.empty () &&
                    ((strvalue[0] < '0') || (strvalue[0] > '9')))
                {
                    if (field == sftransactiontype)
                    {
                        txtype const txtype (txformats::getinstance().
                            findtypebyname (strvalue));

                        ret = std::make_unique <stuint16> (field,
                            static_cast <std::uint16_t> (txtype));

                        if (*name == sfgeneric)
                            name = &sftransaction;
                    }
                    else if (field == sfledgerentrytype)
                    {
                        ledgerentrytype const type (
                            ledgerformats::getinstance().
                                findtypebyname (strvalue));

                        ret = std::make_unique <stuint16> (field,
                            static_cast <std::uint16_t> (type));

                        if (*name == sfgeneric)
                            name = &sfledgerentry;
                    }
                    else
                    {
                        error = invalid_data (json_name, fieldname);
                        return ret;
                    }
                }
                else
                {
                    ret = std::make_unique <stuint16> (field,
                        beast::lexicalcastthrow <std::uint16_t> (strvalue));
                }
            }
            else if (value.isint ())
            {
                ret = std::make_unique <stuint16> (field,
                    range_check_cast <std::uint16_t> (
                        value.asint (), 0, 65535));
            }
            else if (value.isuint ())
            {
                ret = std::make_unique <stuint16> (field,
                    range_check_cast <std::uint16_t> (
                        value.asuint (), 0, 65535));
            }
            else
            {
                error = bad_type (json_name, fieldname);
                return ret;
            }
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }

        break;

    case sti_uint32:
        try
        {
            if (value.isstring ())
            {
                ret = std::make_unique <stuint32> (field,
                    beast::lexicalcastthrow <std::uint32_t> (
                        value.asstring ()));
            }
            else if (value.isint ())
            {
                ret = std::make_unique <stuint32> (field,
                    range_check_cast <std::uint32_t> (
                        value.asint (), 0u, 4294967295u));
            }
            else if (value.isuint ())
            {
                ret = std::make_unique <stuint32> (field,
                    static_cast <std::uint32_t> (value.asuint ()));
            }
            else
            {
                error = bad_type (json_name, fieldname);
                return ret;
            }
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }

        break;

    case sti_uint64:
        try
        {
            if (value.isstring ())
            {
                ret = std::make_unique <stuint64> (field,
                    uintfromhex (value.asstring ()));
            }
            else if (value.isint ())
            {
                ret = std::make_unique <stuint64> (field,
                    range_check_cast<std::uint64_t> (
                        value.asint (), 0, 18446744073709551615ull));
            }
            else if (value.isuint ())
            {
                ret = std::make_unique <stuint64> (field,
                    static_cast <std::uint64_t> (value.asuint ()));
            }
            else
            {
                error = bad_type (json_name, fieldname);
                return ret;
            }
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }

        break;

    case sti_hash128:
        try
        {
            if (value.isstring ())
            {
                ret = std::make_unique <sthash128> (field, value.asstring ());
            }
            else
            {
                error = bad_type (json_name, fieldname);
                return ret;
            }
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }

        break;

    case sti_hash160:
        try
        {
            if (value.isstring ())
            {
                ret = std::make_unique <sthash160> (field, value.asstring ());
            }
            else
            {
                error = bad_type (json_name, fieldname);
                return ret;
            }
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }

        break;

    case sti_hash256:
        try
        {
            if (value.isstring ())
            {
                ret = std::make_unique <sthash256> (field, value.asstring ());
            }
            else
            {
                error = bad_type (json_name, fieldname);
                return ret;
            }
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }

        break;

    case sti_vl:
        if (! value.isstring ())
        {
            error = bad_type (json_name, fieldname);
            return ret;
        }

        try
        {
            std::pair<blob, bool> vblob (strunhex (value.asstring ()));

            if (!vblob.second)
                throw std::invalid_argument ("invalid data");

            ret = std::make_unique <stblob> (field, vblob.first);
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }

        break;

    case sti_amount:
        try
        {
            ret = std::make_unique <stamount> (amountfromjson (field, value));
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }

        break;

    case sti_vector256:
        if (! value.isarray ())
        {
            error = array_expected (json_name, fieldname);
            return ret;
        }

        try
        {
            std::unique_ptr <stvector256> tail =
                std::make_unique <stvector256> (field);
            for (json::uint i = 0; value.isvalidindex (i); ++i)
            {
                uint256 s;
                s.sethex (value[i].asstring ());
                tail->push_back (s);
            }
            ret = std::move (tail);
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }

        break;

    case sti_pathset:
        if (!value.isarray ())
        {
            error = array_expected (json_name, fieldname);
            return ret;
        }

        try
        {
            std::unique_ptr <stpathset> tail =
                std::make_unique <stpathset> (field);

            for (json::uint i = 0; value.isvalidindex (i); ++i)
            {
                stpath p;

                if (!value[i].isarray ())
                {
                    std::stringstream ss;
                    ss << fieldname << "[" << i << "]";
                    error = array_expected (json_name, ss.str ());
                    return ret;
                }

                for (json::uint j = 0; value[i].isvalidindex (j); ++j)
                {
                    std::stringstream ss;
                    ss << fieldname << "[" << i << "][" << j << "]";
                    std::string const element_name (
                        json_name + "." + ss.str());

                    // each element in this path has some combination of
                    // account, currency, or issuer

                    json::value pathel = value[i][j];

                    if (!pathel.isobject ())
                    {
                        error = not_an_object (element_name);
                        return ret;
                    }

                    json::value const& account  = pathel["account"];
                    json::value const& currency = pathel["currency"];
                    json::value const& issuer   = pathel["issuer"];
                    bool hascurrency            = false;
                    account uaccount, uissuer;
                    currency ucurrency;

                    if (! account.isnull ())
                    {
                        // human account id
                        if (! account.isstring ())
                        {
                            error = string_expected (element_name, "account");
                            return ret;
                        }

                        std::string const strvalue (account.asstring ());

                        if (value.size () == 40) // 160-bit hex account value
                            uaccount.sethex (strvalue);

                        {
                            rippleaddress a;

                            if (! a.setaccountid (strvalue))
                            {
                                error = invalid_data (element_name, "account");
                                return ret;
                            }

                            uaccount = a.getaccountid ();
                        }
                    }

                    if (!currency.isnull ())
                    {
                        // human currency
                        if (!currency.isstring ())
                        {
                            error = string_expected (element_name, "currency");
                            return ret;
                        }

                        hascurrency = true;

                        if (currency.asstring ().size () == 40)
                        {
                            ucurrency.sethex (currency.asstring ());
                        }
                        else if (!to_currency (ucurrency, currency.asstring ()))
                        {
                            error = invalid_data (element_name, "currency");
                            return ret;
                        }
                    }

                    if (!issuer.isnull ())
                    {
                        // human account id
                        if (!issuer.isstring ())
                        {
                            error = string_expected (element_name, "issuer");
                            return ret;
                        }

                        if (issuer.asstring ().size () == 40)
                        {
                            uissuer.sethex (issuer.asstring ());
                        }
                        else
                        {
                            rippleaddress a;

                            if (!a.setaccountid (issuer.asstring ()))
                            {
                                error = invalid_data (element_name, "issuer");
                                return ret;
                            }

                            uissuer = a.getaccountid ();
                        }
                    }

                    p.emplace_back (uaccount, ucurrency, uissuer, hascurrency);
                }

                tail->push_back (p);
            }
            ret = std::move (tail);
        }
        catch (...)
        {
            error = invalid_data (json_name, fieldname);
            return ret;
        }

        break;

    case sti_account:
        {
            if (! value.isstring ())
            {
                error = bad_type (json_name, fieldname);
                return ret;
            }

            std::string const strvalue = value.asstring ();

            try
            {
                if (value.size () == 40) // 160-bit hex account value
                {
                    account account;
                    account.sethex (strvalue);
                    ret = std::make_unique <staccount> (field, account);
                }
                else
                {
                    // ripple address
                    rippleaddress a;

                    if (!a.setaccountid (strvalue))
                    {
                        error = invalid_data (json_name, fieldname);
                        return ret;
                    }

                    ret =
                        std::make_unique <staccount> (field, a.getaccountid ());
                }
            }
            catch (...)
            {
                error = invalid_data (json_name, fieldname);
                return ret;
            }
        }
        break;

    default:
        error = bad_type (json_name, fieldname);
        return ret;
    }

    return ret;
}

static const int maxdepth = 64;

// forward declaration since parseobject() and parsearray() call each other.
static bool parsearray (
    std::string const& json_name,
    json::value const& json,
    sfield::ref inname,
    int depth,
    std::unique_ptr <starray>& sub_array,
    json::value& error);



static bool parseobject (
    std::string const& json_name,
    json::value const& json,
    sfield::ref inname,
    int depth,
    std::unique_ptr <stobject>& sub_object,
    json::value& error)
{
    if (! json.isobject ())
    {
        error = not_an_object (json_name);
        return false;
    }

    if (depth > maxdepth)
    {
        error = too_deep (json_name);
        return false;
    }

    sfield::ptr name (&inname);

    boost::ptr_vector<stbase> data;
    json::value::members members (json.getmembernames ());

    for (json::value::members::iterator it (members.begin ());
        it != members.end (); ++it)
    {
        std::string const& fieldname = *it;
        json::value const& value = json [fieldname];

        sfield::ref field = sfield::getfield (fieldname);

        if (field == sfinvalid)
        {
            error = unknown_field (json_name, fieldname);
            return false;
        }

        switch (field.fieldtype)
        {

        // object-style containers (which recurse).
        case sti_object:
        case sti_transaction:
        case sti_ledgerentry:
        case sti_validation:
            if (! value.isobject ())
            {
                error = not_an_object (json_name, fieldname);
                return false;
            }

            try
            {
                std::unique_ptr <stobject> sub_object_;
                bool const success (parseobject (json_name + "." + fieldname,
                    value, field, depth + 1, sub_object_, error));
                if (! success)
                    return false;
                data.push_back (sub_object_.release ());
            }
            catch (...)
            {
                error = invalid_data (json_name, fieldname);
                return false;
            }

            break;

        // array-style containers (which recurse).
        case sti_array:
            try
            {
                std::unique_ptr <starray> sub_array_;
                bool const success (parsearray (json_name + "." + fieldname,
                    value, field, depth + 1, sub_array_, error));
                if (! success)
                    return false;
                data.push_back (sub_array_.release ());
            }
            catch (...)
            {
                error = invalid_data (json_name, fieldname);
                return false;
            }

            break;

        // everything else (types that don't recurse).
        default:
            {
                std::unique_ptr <stbase> sertyp =
                    parseleaf (json_name, fieldname, name, value, error);

                if (!sertyp)
                    return false;

                data.push_back (sertyp.release ());
            }

            break;
        }
    }

    sub_object = std::make_unique <stobject> (*name, data);
    return true;
}

static bool parsearray (
    std::string const& json_name,
    json::value const& json,
    sfield::ref inname,
    int depth,
    std::unique_ptr <starray>& sub_array,
    json::value& error)
{
    if (! json.isarray ())
    {
        error = not_an_array (json_name);
        return false;
    }

    if (depth > maxdepth)
    {
        error = too_deep (json_name);
        return false;
    }

    try
    {
        std::unique_ptr <starray> tail = std::make_unique <starray> (inname);

        for (json::uint i = 0; json.isvalidindex (i); ++i)
        {
            bool const isobject (json[i].isobject());
            bool const singlekey (isobject ? json[i].size() == 1 : true);

            if (!isobject || !singlekey)
            {
                error = singleton_expected (json_name, i);
                return false;
            }

            // todo: there doesn't seem to be a nice way to get just the
            // first/only key in an object without copying all keys into
            // a vector
            std::string const objectname (json[i].getmembernames()[0]);;
            sfield::ref       namefield (sfield::getfield(objectname));

            if (namefield == sfinvalid)
            {
                error = unknown_field (json_name, objectname);
                return false;
            }

            json::value const objectfields (json[i][objectname]);

            std::unique_ptr <stobject> sub_object_;
            {
                std::stringstream ss;
                ss << json_name << "." <<
                    "[" << i << "]." << objectname;
                bool const success (parseobject (ss.str (), objectfields,
                    namefield, depth + 1, sub_object_, error));

                if (! success ||
                    (sub_object_->getfname().fieldtype != sti_object))
                {
                    return false;
                }
            }
            tail->push_back (*sub_object_);
        }
        sub_array = std::move (tail);
    }
    catch (...)
    {
        error = invalid_data (json_name);
        return false;
    }
    return true;
}

} // stparsedjsondetail

//------------------------------------------------------------------------------

stparsedjsonobject::stparsedjsonobject (
    std::string const& name,
    json::value const& json)
{
    using namespace stparsedjsondetail;
    parseobject (name, json, sfgeneric, 0, object, error);
}

//------------------------------------------------------------------------------

stparsedjsonarray::stparsedjsonarray (
    std::string const& name,
    json::value const& json)
{
    using namespace stparsedjsondetail;
    parsearray (name, json, sfgeneric, 0, array, error);
}



} // ripple
