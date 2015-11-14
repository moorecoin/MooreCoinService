//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_utility_propertystream_h_included
#define beast_utility_propertystream_h_included

#include <beast/intrusive/list.h>
#include <beast/threads/shareddata.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

namespace beast {

//------------------------------------------------------------------------------

/** abstract stream with raii containers that produce a property tree. */
class propertystream
{
public:
    class map;
    class set;
    class source;

    propertystream ();
    virtual ~propertystream ();

protected:
    virtual void map_begin () = 0;
    virtual void map_begin (std::string const& key) = 0;
    virtual void map_end () = 0;

    virtual void add (std::string const& key, std::string const& value) = 0;

    void add (std::string const& key, char const* value)
    {
        add (key, std::string (value));
    }

    template <typename value>
    void lexical_add (std::string const &key, value value)
    {
        std::stringstream ss;
        ss << value;
        add (key, ss.str());
    }

    virtual void add (std::string const& key, bool value);
    virtual void add (std::string const& key, char value);
    virtual void add (std::string const& key, signed char value);
    virtual void add (std::string const& key, unsigned char value);
    virtual void add (std::string const& key, wchar_t value);
#if 0
    virtual void add (std::string const& key, char16_t value);
    virtual void add (std::string const& key, char32_t value);
#endif
    virtual void add (std::string const& key, short value);
    virtual void add (std::string const& key, unsigned short value);
    virtual void add (std::string const& key, int value);
    virtual void add (std::string const& key, unsigned int value);
    virtual void add (std::string const& key, long value);
    virtual void add (std::string const& key, unsigned long value);
    virtual void add (std::string const& key, long long value);
    virtual void add (std::string const& key, unsigned long long value);
    virtual void add (std::string const& key, float value);
    virtual void add (std::string const& key, double value);
    virtual void add (std::string const& key, long double value);

    virtual void array_begin () = 0;
    virtual void array_begin (std::string const& key) = 0;
    virtual void array_end () = 0;

    virtual void add (std::string const& value) = 0;

    void add (char const* value)
    {
        add (std::string (value));
    }

    template <typename value>
    void lexical_add (value value)
    {
        std::stringstream ss;
        ss << value;
        add (ss.str());
    }

    virtual void add (bool value);
    virtual void add (char value);
    virtual void add (signed char value);
    virtual void add (unsigned char value);
    virtual void add (wchar_t value);
#if 0
    virtual void add (char16_t value);
    virtual void add (char32_t value);
#endif
    virtual void add (short value);
    virtual void add (unsigned short value);
    virtual void add (int value);
    virtual void add (unsigned int value);
    virtual void add (long value);
    virtual void add (unsigned long value);
    virtual void add (long long value);
    virtual void add (unsigned long long value);
    virtual void add (float value);
    virtual void add (double value);
    virtual void add (long double value);

private:
    class item;
    class proxy;
};

//------------------------------------------------------------------------------
//
// item
//
//------------------------------------------------------------------------------

class propertystream::item : public list <item>::node
{
public:
    explicit item (source* source);
    source& source() const;
    source* operator-> () const;
    source& operator* () const;
private:
    source* m_source;
};

//------------------------------------------------------------------------------
//
// proxy
//
//------------------------------------------------------------------------------

class propertystream::proxy
{
private:
    map const* m_map;
    std::string m_key;
    std::ostringstream mutable m_ostream;

public:
    proxy (map const& map, std::string const& key);
    proxy (proxy const& other);
    ~proxy ();

    template <typename value>
    proxy& operator= (value value);

    std::ostream& operator<< (std::ostream& manip (std::ostream&)) const;

    template <typename t>
    std::ostream& operator<< (t const& t) const
    {
        return m_ostream << t;
    }
};

//------------------------------------------------------------------------------
//
// map
//
//------------------------------------------------------------------------------

class propertystream::map
{
private:
    propertystream& m_stream;

public:
    explicit map (propertystream& stream);
    explicit map (set& parent);
    map (std::string const& key, map& parent);
    map (std::string const& key, propertystream& stream);
    ~map ();

    map(map const&) = delete;
    map& operator= (map const&) = delete;

    propertystream& stream();
    propertystream const& stream() const;

    template <typename value>
    void add (std::string const& key, value value) const
    {
        m_stream.add (key, value);
    }

    template <typename key, typename value>
    void add (key key, value value) const
    {
        std::stringstream ss;
        ss << key;
        add (ss.str(), value);
    }

    proxy operator[] (std::string const& key);    

    proxy operator[] (char const* key)
        { return proxy (*this, key); }

    template <typename key>
    proxy operator[] (key key) const
    {
        std::stringstream ss;
        ss << key;
        return proxy (*this, ss.str());
    }
};

//--------------------------------------------------------------------------

template <typename value>
propertystream::proxy& propertystream::proxy::operator= (value value)
{
    m_map->add (m_key, value);
    return *this;
}

//--------------------------------------------------------------------------
//
// set
//
//------------------------------------------------------------------------------

class propertystream::set
{
private:
    propertystream& m_stream;

public:
    set (std::string const& key, map& map);
    set (std::string const& key, propertystream& stream);
    ~set ();

    set (set const&) = delete;
    set& operator= (set const&) = delete;

    propertystream& stream();
    propertystream const& stream() const;

    template <typename value>
    void add (value value) const
        { m_stream.add (value); }
};

//------------------------------------------------------------------------------
//
// source
//
//------------------------------------------------------------------------------

/** subclasses can be called to write to a stream and have children. */
class propertystream::source
{
private:
    struct state
    {
        explicit state (source* source)
            : item (source)
            , parent (nullptr)
            { }

        item item;
        source* parent;
        list <item> children;
    };

    typedef shareddata <state> sharedstate;

    std::string const m_name;
    sharedstate m_state;

    //--------------------------------------------------------------------------

    void remove    (sharedstate::access& state,
                    sharedstate::access& childstate);

    void removeall (sharedstate::access& state);

    void write     (sharedstate::access& state, propertystream& stream);

public:
    explicit source (std::string const& name);
    ~source ();

    source (source const&) = delete;
    source& operator= (source const&) = delete;

    /** returns the name of this source. */
    std::string const& name() const;

    /** add a child source. */
    void add (source& source);

    /** add a child source by pointer.
        the source pointer is returned so it can be used in ctor-initializers.
    */
    template <class derived>
    derived* add (derived* child)
    {
        add (*static_cast <source*>(child));
        return child;
    }

    /** remove a child source from this source. */
    void remove (source& child);

    /** remove all child sources of this source. */
    void removeall ();

    /** write only this source to the stream. */
    void write_one  (propertystream& stream);

    /** write this source and all its children recursively to the stream. */
    void write      (propertystream& stream);

    /** parse the path and write the corresponding source and optional children.
        if the source is found, it is written. if the wildcard character '*'
        exists as the last character in the path, then all the children are
        written recursively.
    */
    void write      (propertystream& stream, std::string const& path);

    /** parse the dot-delimited source path and return the result.
        the first value will be a pointer to the source object corresponding
        to the given path. if no source object exists, then the first value
        will be nullptr and the second value will be undefined.
        the second value is a boolean indicating whether or not the path string
        specifies the wildcard character '*' as the last character.

        print statement examples
        "parent.child" prints child and all of its children
        "parent.child." start at the parent and print down to child
        "parent.grandchild" prints nothing- grandchild not direct discendent
        "parent.grandchild." starts at the parent and prints down to grandchild
        "parent.grandchild.*" starts at parent, print through grandchild children
    */
    std::pair <source*, bool> find (std::string path);

    source* find_one_deep (std::string const& name);
    propertystream::source* find_path(std::string path);
    propertystream::source* find_one(std::string const& name);

    static bool peel_leading_slash (std::string* path);
    static bool peel_trailing_slashstar (std::string* path);
    static std::string peel_name(std::string* path);    


    //--------------------------------------------------------------------------

    /** subclass override.
        the default version does nothing.
    */
    virtual void onwrite (map&);
};

}

#endif
