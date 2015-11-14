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

#ifndef ripple_protocol_knownformats_h_included
#define ripple_protocol_knownformats_h_included

#include <ripple/protocol/sotemplate.h>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

/** manages a list of known formats.

    each format has a name, an associated keytype (typically an enumeration),
    and a predefined @ref soelement.

    @tparam keytype the type of key identifying the format.
*/
template <class keytype>
class knownformats
{
public:
    /** a known format.
    */
    class item
    {
    public:
        item (char const* name, keytype type)
            : m_name (name)
            , m_type (type)
        {
        }

        item& operator<< (soelement const& el)
        {
            elements.push_back (el);

            return *this;
        }

        /** retrieve the name of the format.
        */
        std::string const& getname () const noexcept
        {
            return m_name;
        }

        /** retrieve the transaction type this format represents.
        */
        keytype gettype () const noexcept
        {
            return m_type;
        }

    public:
        // vfalco todo make an accessor for this
        sotemplate elements;

    private:
        std::string const m_name;
        keytype const m_type;
    };

private:
    typedef std::map <std::string, item*> namemap;
    typedef std::map <keytype, item*> typemap;

public:
    /** create the known formats object.

        derived classes will load the object will all the known formats.
    */
    knownformats ()
    {
    }

    /** destroy the known formats object.

        the defined formats are deleted.
    */
    ~knownformats ()
    {
    }

    /** retrieve the type for a format specified by name.

        if the format name is unknown, an exception is thrown.

        @param  name the name of the type.
        @return      the type.
    */
    keytype findtypebyname (std::string const name) const
    {
        item const* const result = findbyname (name);

        if (result != nullptr)
        {
            return result->gettype ();
        }
        else
        {
            throw std::runtime_error ("unknown format name");
        }
    }

    /** retrieve a format based on its type.
    */
    // vfalco todo can we just return the soelement& ?
    item const* findbytype (keytype type) const noexcept
    {
        item* result = nullptr;

        typename typemap::const_iterator const iter = m_types.find (type);

        if (iter != m_types.end ())
        {
            result = iter->second;
        }

        return result;
    }

protected:
    /** retrieve a format based on its name.
    */
    item const* findbyname (std::string const& name) const noexcept
    {
        item* result = nullptr;

        typename namemap::const_iterator const iter = m_names.find (name);

        if (iter != m_names.end ())
        {
            result = iter->second;
        }

        return result;
    }

    /** add a new format.

        the new format has the set of common fields already added.

        @param name the name of this format.
        @param type the type of this format.

        @return the created format.
    */
    item& add (char const* name, keytype type)
    {
        m_formats.emplace_back (
            std::make_unique <item> (name, type));
        auto& item (*m_formats.back());

        addcommonfields (item);

        m_types [item.gettype ()] = &item;
        m_names [item.getname ()] = &item;

        return item;
    }

    /** adds common fields.

        this is called for every new item.
    */
    virtual void addcommonfields (item& item) = 0;

private:
    knownformats(knownformats const&) = delete;
    knownformats& operator=(knownformats const&) = delete;

    std::vector <std::unique_ptr <item>> m_formats;
    namemap m_names;
    typemap m_types;
};

} // ripple

#endif
