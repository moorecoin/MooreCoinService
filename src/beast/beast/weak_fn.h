//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_weak_fn_h_included
#define beast_weak_fn_h_included

#include <beast/utility/empty_base_optimization.h>
#include <memory>

// original version:
// http://lists.boost.org/archives/boost/att-189469/weak_fn.hpp
//
// this work was adapted from source code with this copyright notice:
//
//  weak_fun.hpp
//
//  copyright (c) 2009 artyom beilis
//
// distributed under the boost software license, version 1.0. (see
// accompanying file license_1_0.txt or copy at
// http://www.boost.org/license_1_0.txt)
//

namespace beast {

// policy throws if weak pointer is expired
template <class v = void>
struct throw_if_invalid
{
    v operator()() const
    {
        throw std::bad_weak_ptr();
    }
};

// policy returns a value if weak pointer is expired
template <class v>
struct return_default_if_invalid
{
    return_default_if_invalid()
        : def_value_()
        { }

    return_default_if_invalid(v def_value)
        : def_value_(def_value)
        { }

    v operator()() const
    {
        return def_value_;
    }

private:
    v def_value_;
};

// policy does nothing if weak pointer is expired
template <class v>
struct ignore_if_invalid
{
    v operator()() const
    {
        return v();
    }
};

template <class v>
using default_invalid_policy = ignore_if_invalid<v>;

namespace detail {

template <class t, class r, class policy, class... args>
class weak_binder
    : private beast::empty_base_optimization<policy>
{
private:
    typedef r (t::*member_type)(args...);
    using pointer_type = std::weak_ptr<t>;
    using shared_type = std::shared_ptr<t>;
    member_type member_;
    pointer_type object_;

public:
    using result_type = r;

    weak_binder (member_type member,
            policy policy, pointer_type object)
        : empty_base_optimization<policy>(std::move(policy))
        , member_(member)
        , object_(object)
        { }

    r operator()(args... args)
    {
        if(auto p = object_.lock())
            return ((*p).*member_)(args...);
        return this->member()();
    }    
};

} // detail

/** returns a callback that can be used with std::bind and a weak_ptr.
    when called, it tries to lock weak_ptr to get a shared_ptr. if successful,
    it calls given member function with given arguments. if not successful,
    the policy functor is called. built-in policies are:
    
    ignore_if_invalid           does nothing
    throw_if_invalid            throws `bad_weak_ptr`
    return_default_if_invalid   returns a chosen value
    
    example:

    struct foo {
        void bar(int i) {
            std::cout << i << std::endl;
        }
    };

    struct do_something {
        void operator()() {
            std::cout << "outdated reference" << std::endl;
        }
    };
    
    int main()
    {
        std::shared_ptr<foo> sp(new foo());
        std::weak_ptr<foo> wp(sp);
    
        std::bind(weak_fn(&foo::bar, wp), _1)(1);
        sp.reset();
        std::bind(weak_fn(&foo::bar, wp), 1)();
        std::bind(weak_fn(&foo::bar, wp, do_something()), 1)();
    }
*/
/** @{ */
template <class t, class r, class policy, class... args>
detail::weak_binder<t, r, policy, args...>
weak_fn (r (t::*member)(args...), std::shared_ptr<t> p,
    policy policy)
{
    return detail::weak_binder<t, r,
        policy, args...>(member, policy, p);
}

template <class t, class r, class... args>
detail::weak_binder<t, r, default_invalid_policy<r>, args...>
weak_fn (r (t::*member)(args...), std::shared_ptr<t> p)
{
    return detail::weak_binder<t, r,
        default_invalid_policy<r>, args...>(member,
            default_invalid_policy<r>{}, p);
}
/** @} */

} // beast

#endif
