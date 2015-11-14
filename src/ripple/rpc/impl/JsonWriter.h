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

#ifndef rippled_ripple_basics_types_jsonwriter_h
#define rippled_ripple_basics_types_jsonwriter_h

#include <ripple/basics/tostring.h>
#include <ripple/protocol/errorcodes.h>
#include <ripple/rpc/output.h>
#include <ripple/protocol/errorcodes.h>

namespace ripple {
namespace rpc {

/**
 *  writer implements an o(1)-space, o(1)-granular output json writer.
 *
 *  o(1)-space means that it uses a fixed amount of memory, and that there are
 *  no heap allocations at each step of the way.
 *
 *  o(1)-granular output means the writer only outputs in small segments of a
 *  bounded size, using a bounded number of cpu cycles in doing so.  this is
 *  very helpful in scheduling long jobs.
 *
 *  the tradeoff is that you have to fill items in the json tree as you go,
 *  and you can never go backward.
 *
 *  writer can write single json tokens, but the typical use is to write out an
 *  entire json object.  for example:
 *
 *      {
 *          writer w (out);
 *
 *          w.startobject ();          // start the root object.
 *          w.set ("hello", "world");
 *          w.set ("goodbye", 23);
 *          w.finishobject ();         // finish the root object.
 *      }
 *
 *  which outputs the string
 *
 *      {"hello":"world","goodbye":23}
 *
 *  there can be an object inside an object:
 *
 *      {
 *          writer w (out);
 *
 *          w.startobject ();                // start the root object.
 *          w.set ("hello", "world");
 *
 *          w.startobjectset ("subobject");  // start a sub-object.
 *          w.set ("goodbye", 23);           // add a key, value assignment.
 *          w.finishobject ();               // finish the sub-object.
 *
 *          w.finishobject ();               // finish the root-object.
 *      }
 *
 *  which outputs the string
 *
 *     {"hello":"world","subobject":{"goodbye":23}}.
 *
 *  arrays work similarly
 *
 *      {
 *          writer w (out);
 *          w.startobject ();           // start the root object.
 *
 *          w.startarrayset ("hello");  // start an array.
 *          w.append (23)               // append some items.
 *          w.append ("skidoo")
 *          w.finisharray ();           // finish the array.
 *
 *          w.finishobject ();          // finish the root object.
 *      }
 *
 *  which outputs the string
 *
 *      {"hello":[23,"skidoo"]}.
 *
 *
 *  if you've reached the end of a long object, you can just use finishall()
 *  which finishes all arrays and objects that you have started.
 *
 *      {
 *          writer w (out);
 *          w.startobject ();           // start the root object.
 *
 *          w.startarrayset ("hello");  // start an array.
 *          w.append (23)               // append an item.
 *
 *          w.startarrayappend ()       // start a sub-array.
 *          w.append ("one");
 *          w.append ("two");
 *
 *          w.startobjectappend ();     // append a sub-object.
 *          w.finishall ();             // finish everything.
 *      }
 *
 *  which outputs the string
 *
 *      {"hello":[23,["one","two",{}]]}.
 *
 *  for convenience, the destructor of writer calls w.finishall() which makes
 *  sure that all arrays and objects are closed.  this means that you can throw
 *  an exception, or have a coroutine simply clean up the stack, and be sure
 *  that you do in fact generate a complete json object.
 */

class writer
{
public:
    enum collectiontype {array, object};

    explicit writer (output const& output);
    writer(writer&&);
    writer& operator=(writer&&);

    ~writer();

    /** start a new collection at the root level. */
    void startroot (collectiontype);

    /** start a new collection inside an array. */
    void startappend (collectiontype);

    /** start a new collection inside an object. */
    void startset (collectiontype, std::string const& key);

    /** finish the collection most recently started. */
    void finish ();

    /** finish all objects and arrays.  after finisharray() has been called, no
     *  more operations can be performed. */
    void finishall ();

    /** append a value to an array.
     *
     *  scalar must be a scalar - that is, a number, boolean, string, string
     *  literal, nullptr or json::value
     */
    template <typename scalar>
    void append (scalar t)
    {
        rawappend();
        output (t);
    }

    /** add a comma before this next item if not the first item in an array.
        useful if you are writing the actual array yourself. */
    void rawappend();

    /** add a key, value assignment to an object.
     *
     *  scalar must be a scalar - that is, a number, boolean, string, string
     *  literal, or nullptr.
     *
     *  while the json spec doesn't explicitly disallow this, you should avoid
     *  calling this method twice with the same tag for the same object.
     *
     *  if check_json_writer is defined, this function throws an exception if if
     *  the tag you use has already been used in this object.
     */
    template <typename type>
    void set (std::string const& tag, type t)
    {
        rawset (tag);
        output (t);
    }

    /** emit just "tag": as part of an object.  useful if you are writing the
        actual value data yourself. */
    void rawset (std::string const& key);

    // you won't need to call anything below here until you are writing single
    // items (numbers, strings, bools, null) to a json stream.

    /*** output a string. */
    void output (std::string const&);

    /*** output a literal constant or c string. */
    void output (char const*);

    /*** output a literal constant or c string. */
    void output (json::value const&);

    /** output numbers, booleans, or nullptr. */
    template <typename type>
    void output (type t)
    {
        imploutput (to_string (t));
    }

    /** output an error code. */
    void output (error_code_i t)
    {
        output (int(t));
    }

    void output (json::staticstring const& t)
    {
        output (t.c_str());
    }

private:
    class impl;
    std::unique_ptr <impl> impl_;

    void imploutput (std::string const&);
};

class jsonexception : public std::exception
{
public:
    explicit jsonexception (std::string const& name) : name_(name) {}
    const char* what() const throw() override
    {
        return name_.c_str();
    }

private:
    std::string const name_;
};

inline void check (bool condition, std::string const& message)
{
    if (!condition)
        throw jsonexception (message);
}

} // rpc
} // ripple

#endif
