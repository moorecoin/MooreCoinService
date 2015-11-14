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

#ifndef beast_utility_staticobject_h_included
#define beast_utility_staticobject_h_included

#include <atomic>
#include <chrono>
#include <thread>

namespace beast {

// spec: n2914=09-0104
//
// [3.6.2] initialization of non-local objects
//
//         objects with static storage duration (3.7.1) or thread storage
//         duration (3.7.2) shall be zero-initialized (8.5) before any
//         other initialization takes place.
//

/** wrapper to produce an object with static storage duration.
    
    the object is constructed in a thread-safe fashion when the get function
    is first called. note that the destructor for object is never called. to
    invoke the destructor, use the atexithook facility (with caution).

    the tag parameter allows multiple instances of the same object type, by
    using different tags.

    object must meet these requirements:
        defaultconstructible

    @see atexithook
*/
template <class object, typename tag = void>
class staticobject
{
public:
    static object& get ()
    {
        staticdata& staticdata (staticdata::get());

        if (staticdata.state.load () != initialized)
        {
            if (staticdata.state.exchange (initializing) == uninitialized)
            {
                // initialize the object.
                new (&staticdata.object) object;
                staticdata.state = initialized;
            }
            else
            {
                std::size_t n = 0;

                while (staticdata.state.load () != initialized)
                {
                    ++n;

                    std::this_thread::yield ();

                    if (n > 10)
                    {
                        std::chrono::milliseconds duration (1);

                        if (n > 100)
                            duration *= 10;

                        std::this_thread::sleep_for (duration);
                    }
                }
            }
        }

        assert (staticdata.state.load () == initialized);
        return staticdata.object;
    }

private:
    static int const uninitialized = 0;
    static int const initializing = 1;
    static int const initialized = 2;

    // this structure gets zero-filled at static initialization time.
    // no constructors are called.
    //
    class staticdata
    {
    public:
        std::atomic <int> state;
        object object;

        static staticdata& get ()
        {
            static std::uint8_t storage [sizeof (staticdata)];
            return *(reinterpret_cast <staticdata*> (&storage [0]));
        }

        staticdata() = delete;
        staticdata(staticdata const&) = delete;
        staticdata& operator= (staticdata const&) = delete;
        ~staticdata() = delete;
    };
};

}

#endif
