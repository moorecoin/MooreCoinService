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

#include <beast/utility/propertystream.h>
#include <beast/unit_test/suite.h>

#include <limits>
#include <iostream>

namespace beast {

//------------------------------------------------------------------------------
//
// item
//
//------------------------------------------------------------------------------

propertystream::item::item (source* source)
    : m_source (source)
{
}

propertystream::source& propertystream::item::source() const
{
    return *m_source;
}

propertystream::source* propertystream::item::operator-> () const
{
    return &source();
}

propertystream::source& propertystream::item::operator* () const
{
    return source();
}

//------------------------------------------------------------------------------
//
// proxy
//
//------------------------------------------------------------------------------

propertystream::proxy::proxy (
    map const& map, std::string const& key)
    : m_map (&map)
    , m_key (key)
{
}

propertystream::proxy::proxy (proxy const& other)
    : m_map (other.m_map)
    , m_key (other.m_key)
{
}

propertystream::proxy::~proxy ()
{
    std::string const s (m_ostream.str());
    if (! s.empty())
        m_map->add (m_key, s);
}

std::ostream& propertystream::proxy::operator<< (
    std::ostream& manip (std::ostream&)) const
{
    return m_ostream << manip;
}

//------------------------------------------------------------------------------
//
// map
//
//------------------------------------------------------------------------------

propertystream::map::map (propertystream& stream)
    : m_stream (stream)
{
}

propertystream::map::map (set& parent)
    : m_stream (parent.stream())
{
    m_stream.map_begin ();
}

propertystream::map::map (std::string const& key, map& map)
    : m_stream (map.stream())
{
    m_stream.map_begin (key);
}

propertystream::map::map (std::string const& key, propertystream& stream)
    : m_stream (stream)
{
    m_stream.map_begin (key);
}

propertystream::map::~map ()
{
    m_stream.map_end ();
}

propertystream& propertystream::map::stream()
{
    return m_stream;
}

propertystream const& propertystream::map::stream() const
{
    return m_stream;
}

propertystream::proxy propertystream::map::operator[] (std::string const& key)
{
    return proxy (*this, key);
}

//------------------------------------------------------------------------------
//
// set
//
//------------------------------------------------------------------------------

propertystream::set::set (std::string const& key, map& map)
    : m_stream (map.stream())
{
    m_stream.array_begin (key);
}

propertystream::set::set (std::string const& key, propertystream& stream)
    : m_stream (stream)
{
    m_stream.array_begin (key);
}

propertystream::set::~set ()
{
    m_stream.array_end ();
}

propertystream& propertystream::set::stream()
{
    return m_stream;
}

propertystream const& propertystream::set::stream() const
{
    return m_stream;
}

//------------------------------------------------------------------------------
//
// source
//
//------------------------------------------------------------------------------

propertystream::source::source (std::string const& name)
    : m_name (name)
    , m_state (this)
{
}

propertystream::source::~source ()
{
    sharedstate::access state (m_state);
    if (state->parent != nullptr)
        state->parent->remove (*this);
    removeall (state);
}

std::string const& propertystream::source::name () const
{
    return m_name;
}

void propertystream::source::add (source& source)
{
    sharedstate::access state (m_state);
    sharedstate::access childstate (source.m_state);
    bassert (childstate->parent == nullptr);
    state->children.push_back (childstate->item);
    childstate->parent = this;
}

void propertystream::source::remove (source& child)
{
    sharedstate::access state (m_state);
    sharedstate::access childstate (child.m_state);
    remove (state, childstate);
}

void propertystream::source::removeall ()
{
    sharedstate::access state (m_state);
    removeall (state);
}

//------------------------------------------------------------------------------

void propertystream::source::write (
    sharedstate::access& state, propertystream &stream)
{
    for (list <item>::iterator iter (state->children.begin());
        iter != state->children.end(); ++iter)
    {
        source& source (iter->source());
        map map (source.name(), stream);
        source.write (stream);
    }
}

//------------------------------------------------------------------------------

void propertystream::source::write_one (propertystream& stream)
{
    map map (m_name, stream);
    onwrite (map);
}

void propertystream::source::write (propertystream& stream)
{
    map map (m_name, stream);
    onwrite (map);

    sharedstate::access state (m_state);

    for (list <item>::iterator iter (state->children.begin());
        iter != state->children.end(); ++iter)
    {
        source& source (iter->source());
        source.write (stream);
    }
}

void propertystream::source::write (propertystream& stream, std::string const& path)
{
    std::pair <source*, bool> result (find (path));

    if (result.first == nullptr)
        return;

    if (result.second)
        result.first->write (stream);
    else
        result.first->write_one (stream);
}

std::pair <propertystream::source*, bool> propertystream::source::find (std::string path)
{
    bool const deep (peel_trailing_slashstar (&path));
    bool const rooted (peel_leading_slash (&path));
    source* source (this);
    if (! path.empty())
    {
        if (! rooted)
        {
            std::string const name (peel_name (&path));
            source = find_one_deep (name);
            if (source == nullptr)
                return std::make_pair (nullptr, deep);
        }
        source = source->find_path (path);
    }
    return std::make_pair (source, deep);
}

bool propertystream::source::peel_leading_slash (std::string* path)
{
    if (! path->empty() && path->front() == '/')
    {
        *path = std::string (path->begin() + 1, path->end());
        return true;
    }
    return false;
}

bool propertystream::source::peel_trailing_slashstar (std::string* path)
{
    bool found(false);
    if (path->empty())
        return false;
    if (path->back() == '*')
    {
        found = true;
        path->pop_back();
    }
    if(! path->empty() && path->back() == '/')
        path->pop_back();
    return found;
}

std::string propertystream::source::peel_name (std::string* path)
{
    if (path->empty())
        return "";
    
    std::string::const_iterator first = (*path).begin();
    std::string::const_iterator last = (*path).end();
    std::string::const_iterator pos (std::find (first, last, '/'));
    std::string s (first, pos);
    
    if (pos != last)
        *path = std::string (pos+1, last);
    else
        *path = std::string ();

    return s;
}

// recursive search through the whole tree until name is found
propertystream::source* propertystream::source::find_one_deep (std::string const& name)
{
    source* found = find_one (name);
    if (found != nullptr)
        return found;
    sharedstate::access state (this->m_state);
    for (auto& s : state->children)
    {
        found = s.source().find_one_deep (name);
        if (found != nullptr)
            return found;
    }
    return nullptr;
}

propertystream::source* propertystream::source::find_path (std::string path)
{
    if (path.empty())
        return this;
    source* source (this);
    do
    {
        std::string const name (peel_name (&path));
        if(name.empty ())
            break;
        source = source->find_one(name);
    }
    while (source != nullptr); 
    return source;
}

// this function only looks at immediate children
// if no immediate children match, then return nullptr
propertystream::source* propertystream::source::find_one (std::string const& name)
{
    sharedstate::access state (this->m_state);
    for (auto& s : state->children)
    {
        if (s.source().m_name == name)
            return &s.source();
    }
    return nullptr;
}

void propertystream::source::onwrite (map&)
{
}

//------------------------------------------------------------------------------

void propertystream::source::remove (
    sharedstate::access& state, sharedstate::access& childstate)
{
    bassert (childstate->parent == this);
    state->children.erase (
        state->children.iterator_to (
            childstate->item));
    childstate->parent = nullptr;
}

void propertystream::source::removeall (sharedstate::access& state)
{
    for (list <item>::iterator iter (state->children.begin());
        iter != state->children.end();)
    {
        sharedstate::access childstate ((*iter)->m_state);
        remove (state, childstate);
    }
}

//------------------------------------------------------------------------------
//
// propertystream
//
//------------------------------------------------------------------------------

propertystream::propertystream ()
{
}

propertystream::~propertystream ()
{
}

void propertystream::add (std::string const& key, bool value)
{
    if (value)
        add (key, "true");
    else
        add (key, "false");
}

void propertystream::add (std::string const& key, char value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, signed char value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, unsigned char value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, wchar_t value)
{
    lexical_add (key, value);
}

#if 0
void propertystream::add (std::string const& key, char16_t value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, char32_t value)
{
    lexical_add (key, value);
}
#endif

void propertystream::add (std::string const& key, short value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, unsigned short value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, int value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, unsigned int value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, long value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, unsigned long value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, long long value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, unsigned long long value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, float value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, double value)
{
    lexical_add (key, value);
}

void propertystream::add (std::string const& key, long double value)
{
    lexical_add (key, value);
}

void propertystream::add (bool value)
{
    if (value)
        add ("true");
    else
        add ("false");
}

void propertystream::add (char value)
{
    lexical_add (value);
}

void propertystream::add (signed char value)
{
    lexical_add (value);
}

void propertystream::add (unsigned char value)
{
    lexical_add (value);
}

void propertystream::add (wchar_t value)
{
    lexical_add (value);
}

#if 0
void propertystream::add (char16_t value)
{
    lexical_add (value);
}

void propertystream::add (char32_t value)
{
    lexical_add (value);
}
#endif

void propertystream::add (short value)
{
    lexical_add (value);
}

void propertystream::add (unsigned short value)
{
    lexical_add (value);
}

void propertystream::add (int value)
{
    lexical_add (value);
}

void propertystream::add (unsigned int value)
{
    lexical_add (value);
}

void propertystream::add (long value)
{
    lexical_add (value);
}

void propertystream::add (unsigned long value)
{
    lexical_add (value);
}

void propertystream::add (long long value)
{
    lexical_add (value);
}

void propertystream::add (unsigned long long value)
{
    lexical_add (value);
}

void propertystream::add (float value)
{
    lexical_add (value);
}

void propertystream::add (double value)
{
    lexical_add (value);
}

void propertystream::add (long double value)
{
    lexical_add (value);
}

//------------------------------------------------------------------------------

class propertystream_test : public unit_test::suite
{
public:
    typedef propertystream::source source;

    void test_peel_name (std::string s, std::string const& expected,
        std::string const& expected_remainder)
    {
        try
        {
            std::string const peeled_name = source::peel_name (&s);
            expect (peeled_name == expected);
            expect (s == expected_remainder);
        }
        catch (...)
        {
            fail ("unhandled exception");;
       }
    }

    void test_peel_leading_slash (std::string s, std::string const& expected,
        bool should_be_found)
    {
        try
        {
            bool const found (source::peel_leading_slash (&s));
            expect (found == should_be_found);
            expect (s == expected);
        }
        catch(...)
        {
            fail ("unhandled exception");;
        }
    }

    void test_peel_trailing_slashstar (std::string s, 
        std::string const& expected_remainder, bool should_be_found)
    {
        try
        {
            bool const found (source::peel_trailing_slashstar (&s));
            expect (found == should_be_found);
            expect (s == expected_remainder);
        }
        catch (...)
        {
            fail ("unhandled exception");;
        }  
    }

    void test_find_one (source& root, source* expected, std::string const& name)
    {
        try
        {
            source* source (root.find_one (name));
            expect (source == expected);
        }
        catch (...)
        {
            fail ("unhandled exception");;
        }
    }

    void test_find_path (source& root, std::string const& path,
        source* expected)
    {
        try
        {
            source* source (root.find_path (path));
            expect (source == expected);
        }
        catch (...)
        {
            fail ("unhandled exception");;
        }
    }

    void test_find_one_deep (source& root, std::string const& name,
        source* expected)
    {
        try
        {
            source* source (root.find_one_deep (name));
            expect (source == expected);
        }
        catch(...)
        {
            fail ("unhandled exception");;
        }
    }

    void test_find (source& root, std::string path, source* expected, 
        bool expected_star)
    {
        try
        {
            auto const result (root.find (path));
            expect (result.first == expected);
            expect (result.second == expected_star);
        }
        catch (...)
        {
            fail ("unhandled exception");;
        }
    }

    void run()
    {
        source a ("a");
        source b ("b");
        source c ("c");
        source d ("d");
        source e ("e");
        source f ("f");
        source g ("g");

        //
        // a { b { d { f }, e }, c { g } }
        //

        a.add ( b );
        a.add ( c );
        c.add ( g );
        b.add ( d );
        b.add ( e );
        d.add ( f );

        testcase ("peel_name");
        test_peel_name ("a", "a", "");
        test_peel_name ("foo/bar", "foo", "bar");
        test_peel_name ("foo/goo/bar", "foo", "goo/bar");
        test_peel_name ("", "", "");

        testcase ("peel_leading_slash");
        test_peel_leading_slash ("foo/", "foo/", false);
        test_peel_leading_slash ("foo", "foo", false);
        test_peel_leading_slash ("/foo/", "foo/", true);
        test_peel_leading_slash ("/foo", "foo", true);

        testcase ("peel_trailing_slashstar");
        test_peel_trailing_slashstar ("/foo/goo/*", "/foo/goo", true);
        test_peel_trailing_slashstar ("foo/goo/*", "foo/goo", true);
        test_peel_trailing_slashstar ("/foo/goo/", "/foo/goo", false);
        test_peel_trailing_slashstar ("foo/goo", "foo/goo", false);
        test_peel_trailing_slashstar ("", "", false);
        test_peel_trailing_slashstar ("/", "", false);
        test_peel_trailing_slashstar ("/*", "", true);
        test_peel_trailing_slashstar ("//", "/", false);
        test_peel_trailing_slashstar ("**", "*", true);
        test_peel_trailing_slashstar ("*/", "*", false);

        testcase ("find_one");
        test_find_one (a, &b, "b");
        test_find_one (a, nullptr, "d");
        test_find_one (b, &e, "e");
        test_find_one (d, &f, "f");

        testcase ("find_path");
        test_find_path (a, "a", nullptr); 
        test_find_path (a, "e", nullptr); 
        test_find_path (a, "a/b", nullptr);
        test_find_path (a, "a/b/e", nullptr);
        test_find_path (a, "b/e/g", nullptr);
        test_find_path (a, "b/e/f", nullptr);
        test_find_path (a, "b", &b);
        test_find_path (a, "b/e", &e);
        test_find_path (a, "b/d/f", &f);
        
        testcase ("find_one_deep");
        test_find_one_deep (a, "z", nullptr);
        test_find_one_deep (a, "g", &g);
        test_find_one_deep (a, "b", &b);
        test_find_one_deep (a, "d", &d);
        test_find_one_deep (a, "f", &f);

        testcase ("find");
        test_find (a, "",     &a, false);
        test_find (a, "*",    &a, true);
        test_find (a, "/b",   &b, false);
        test_find (a, "b",    &b, false);
        test_find (a, "d",    &d, false);
        test_find (a, "/b*",  &b, true);
        test_find (a, "b*",   &b, true);
        test_find (a, "d*",   &d, true);
        test_find (a, "/b/*", &b, true);
        test_find (a, "b/*",  &b, true);
        test_find (a, "d/*",  &d, true);
        test_find (a, "a",    nullptr, false);
        test_find (a, "/d",   nullptr, false);
        test_find (a, "/d*",  nullptr, true);
        test_find (a, "/d/*", nullptr, true);
    }
};

beast_define_testsuite(propertystream,utility,beast);

}

