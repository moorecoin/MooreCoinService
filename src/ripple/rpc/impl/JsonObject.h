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

#ifndef rippled_ripple_rpc_impl_jsoncollections_h
#define rippled_ripple_rpc_impl_jsoncollections_h

#include <ripple/rpc/impl/jsonwriter.h>

namespace ripple {
namespace rpc {

/**
    collection is a base class for array and object, classes which provide the
    facade of json collections for the o(1) json writer, while still using no
    heap memory and only a very small amount of stack.

    from http://json.org, json has two types of collection: array, and object.
    everything else is a *scalar* - a number, a string, a boolean, the special
    value null, or a legacy json::value.

    collections must write json "as-it-goes" in order to get the strong
    performance guarantees.  this puts restrictions upon api users:

    1. only one collection can be open for change at any one time.

       this condition is enforced automatically and a jsonexception thrown if it
       is violated.

    2. a tag may only be used once in an object.

       some objects have many tags, so this condition might be a little
       expensive. enforcement of this condition is turned on in debug builds and
       a jsonexception is thrown when the tag is added for a second time.

    code samples:

        writer writer;

        // an empty object.
        {
            object::root (writer);
        }
        // outputs {}

        // an object with one scalar value.
        {
            object::root root (writer);
            write["hello"] = "world";
        }
        // outputs {"hello":"world"}

        // same, using chaining.
        {
            object::root (writer)["hello"] = "world";
        }
        // output is the same.

        // add several scalars, with chaining.
        {
            object::root (writer)
                .set ("hello", "world")
                .set ("flag", false)
                .set ("x", 42);
        }
        // outputs {"hello":"world","flag":false,"x":42}

        // add an array.
        {
            object::root root (writer);
            {
               auto array = root.makearray ("hands");
               array.append ("left");
               array.append ("right");
            }
        }
        // outputs {"hands":["left", "right"]}

        // same, using chaining.
        {
            object::root (writer)
                .makearray ("hands")
                .append ("left")
                .append ("right");
        }
        // output is the same.

        // add an object.
        {
            object::root root (writer);
            {
               auto object = root.makeobject ("hands");
               object["left"] = false;
               object["right"] = true;
            }
        }
        // outputs {"hands":{"left":false,"right":true}}

        // same, using chaining.
        {
            object::root (writer)
                .makeobject ("hands")
                .set ("left", false)
                .set ("right", true);
            }
        }
        // outputs {"hands":{"left":false,"right":true}}


   typical ways to make mistakes and get a jsonexception:

        writer writer;
        object::root root (writer);

        // repeat a tag.
        {
            root ["hello"] = "world";
            root ["hello"] = "there";  // throws! in a debug build.
        }

        // open a subcollection, then set something else.
        {
            auto object = root.makeobject ("foo");
            root ["hello"] = "world";  // throws!
        }

        // open two subcollections at a time.
        {
            auto object = root.makeobject ("foo");
            auto array = root.makearray ("bar");  // throws!!
        }

   for more examples, check the unit tests.
 */

class collection
{
public:
    collection (collection&& c);
    collection& operator= (collection&& c);
    collection() = delete;

    ~collection();

protected:
    // a null parent means "no parent at all".
    // writers cannot be null.
    collection (collection* parent, writer*);
    void checkwritable (std::string const& label);

    collection* parent_;
    writer* writer_;
    bool enabled_;
};

class array;

//------------------------------------------------------------------------------

/** represents a json object being written to a writer. */
class object : protected collection
{
public:
    /** object::root is the only collection that has a public constructor. */
    class root;

    /** set a scalar value in the object for a key.

        a json scalar is a single value - a number, string, boolean, nullptr or
        a json::value.

        `set()` throws an exception if this object is disabled (which means that
        one of its children is enabled).

        in a debug build, `set()` also throws an exception if the key has
        already been set() before.

        an operator[] is provided to allow writing `object["key"] = scalar;`.
     */
    template <typename scalar>
    object& set (std::string const& key, scalar const&);

    // detail class and method used to implement operator[].
    class proxy;

    proxy operator[] (std::string const& key);
    proxy operator[] (json::staticstring const& key);

    /** make a new object at a key and return it.

        this object is disabled until that sub-object is destroyed.
        throws an exception if this object was already disabled.
     */
    object makeobject (std::string const& key);

    /** make a new array at a key and return it.

        this object is disabled until that sub-array is destroyed.
        throws an exception if this object was already disabled.
     */
    array makearray (std::string const& key);

protected:
    friend class array;
    object (collection* parent, writer* w) : collection (parent, w) {}
};

class object::root : public object
{
  public:
    /** each object::root must be constructed with its own unique writer. */
    root (writer&);
};

//------------------------------------------------------------------------------

/** represents a json array being written to a writer. */
class array : private collection
{
public:
    /** append a scalar to the arrary.

        throws an exception if this array is disabled (which means that one of
        its sub-collections is enabled).
    */
    template <typename scalar>
    array& append (const scalar&);

    /** append a new object and return it.

        this array is disabled until that sub-object is destroyed.
        throws an exception if this array was already disabled.
     */
    object makeobject ();

    /** append a new array and return it.

        this array is disabled until that sub-array is destroyed.
        throws an exception if this array was already disabled.
     */
    array makearray ();

  protected:
    friend class object;
    array (collection* parent, writer* w) : collection (parent, w) {}
};

//------------------------------------------------------------------------------

// generic accessor functions to allow json::value and collection to
// interoperate.

/** add a new subarray at a named key in a json object. */
json::value& addarray (json::value&, json::staticstring const& key);

/** add a new subarray at a named key in a json object. */
array addarray (object&, json::staticstring const& key);

/** add a new subobject at a named key in a json object. */
json::value& addobject (json::value&, json::staticstring const& key);

/** add a new subobject at a named key in a json object. */
object addobject (object&, json::staticstring const& key);

/** copy all the keys and values from one object into another. */
void copyfrom (json::value& to, json::value const& from);

/** copy all the keys and values from one object into another. */
void copyfrom (object& to, json::value const& from);

/** an object that contains its own writer. */
class writerobject
{
public:
    writerobject (output const& output)
            : writer_ (std::make_unique<writer> (output)),
              object_ (std::make_unique<object::root> (*writer_))
    {
    }

#ifdef _msc_ver
    writerobject (writerobject&& other)
            : writer_ (std::move (other.writer_)),
              object_ (std::move (other.object_))
    {
    }
#endif

    object* operator->()
    {
        return object_.get();
    }

    object& operator*()
    {
        return *object_;
    }

private:
    std::unique_ptr <writer> writer_;
    std::unique_ptr<object::root> object_;
};

writerobject stringwriterobject (std::string&);

//------------------------------------------------------------------------------
// implementation details.

// detail class for object::operator[].
class object::proxy
{
private:
    object& object_;
    std::string const key_;

public:
    proxy (object& object, std::string const& key);

    template <class t>
    object& operator= (t const& t)
    {
        object_.set (key_, t);
        return object_;
    }
};

//------------------------------------------------------------------------------

template <typename scalar>
array& array::append (scalar const& value)
{
    checkwritable ("append");
    if (writer_)
        writer_->append (value);
    return *this;
}

template <typename scalar>
object& object::set (std::string const& key, scalar const& value)
{
    checkwritable ("set");
    if (writer_)
        writer_->set (key, value);
    return *this;
}

inline
json::value& addarray (json::value& json, json::staticstring const& key)
{
    return (json[key] = json::arrayvalue);
}

inline
array addarray (object& json, json::staticstring const& key)
{
    return json.makearray (std::string (key));
}

inline
json::value& addobject (json::value& json, json::staticstring const& key)
{
    return (json[key] = json::objectvalue);
}

inline
object addobject (object& object, json::staticstring const& key)
{
    return object.makeobject (std::string (key));
}

} // rpc
} // ripple

#endif
