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

#ifndef beast_utility_iscallpossible_h_included
#define beast_utility_iscallpossible_h_included

#include <beast/cxx14/type_traits.h> // <type_traits>

namespace beast {

// inspired by roman perepelitsa's presentation from comp.lang.c++.moderated
// based on the implementation here: http://www.rsdn.ru/forum/cpp/2759773.1.aspx
//
namespace is_call_possible_detail
{
    template<typename z>
    struct add_reference
    {
      typedef z& type;
    };

    template<typename z>
    struct add_reference<z&>
    {
      typedef z& type;
    };

   template <typename z> class void_exp_result {}; 

   template <typename z, typename u> 
   u const& operator,(u const&, void_exp_result<z>); 

   template <typename z, typename u> 
   u& operator,(u&, void_exp_result<z>); 

   template <typename src_type, typename dest_type> 
   struct clone_constness 
   { 
     typedef dest_type type; 
   }; 

   template <typename src_type, typename dest_type> 
   struct clone_constness<const src_type, dest_type> 
   { 
     typedef const dest_type type; 
   }; 
}

#define beast_define_has_member_function(trait_name, member_function_name)                                   \
template<typename z, typename iscallpossiblesignature> class trait_name;                                     \
                                                                                                             \
template<typename z, typename result>                                                                        \
class trait_name<z, result(void)>                                                                            \
{                                                                                                            \
   class yes { char m; };                                                                                    \
   class no { yes m[2]; };                                                                                   \
   struct base_mixin                                                                                         \
   {                                                                                                         \
     result member_function_name();                                                                          \
   };                                                                                                        \
   struct base : public z, public base_mixin { private: base(); };                                           \
   template <typename u, u t>  class helper{};                                                               \
   template <typename u>                                                                                     \
   static no deduce(u*, helper<result (base_mixin::*)(), &u::member_function_name>* = 0);                    \
   static yes deduce(...);                                                                                   \
public:                                                                                                      \
   static const bool value = sizeof(yes) == sizeof(deduce(static_cast<base*>(0)));                           \
};                                                                                                           \
                                                                                                             \
template<typename z, typename result, typename arg>                                                          \
class trait_name<z, result(arg)>                                                                             \
{                                                                                                            \
   class yes { char m; };                                                                                    \
   class no { yes m[2]; };                                                                                   \
   struct base_mixin                                                                                         \
   {                                                                                                         \
     result member_function_name(arg);                                                                       \
   };                                                                                                        \
   struct base : public z, public base_mixin { private: base(); };                                           \
   template <typename u, u t>  class helper{};                                                               \
   template <typename u>                                                                                     \
   static no deduce(u*, helper<result (base_mixin::*)(arg), &u::member_function_name>* = 0);                 \
   static yes deduce(...);                                                                                   \
public:                                                                                                      \
   static const bool value = sizeof(yes) == sizeof(deduce(static_cast<base*>(0)));                           \
};                                                                                                           \
                                                                                                             \
template<typename z, typename result, typename arg1, typename arg2>                                          \
class trait_name<z, result(arg1,arg2)>                                                                       \
{                                                                                                            \
   class yes { char m; };                                                                                    \
   class no { yes m[2]; };                                                                                   \
   struct base_mixin                                                                                         \
   {                                                                                                         \
     result member_function_name(arg1,arg2);                                                                 \
   };                                                                                                        \
   struct base : public z, public base_mixin { private: base(); };                                           \
   template <typename u, u t>  class helper{};                                                               \
   template <typename u>                                                                                     \
   static no deduce(u*, helper<result (base_mixin::*)(arg1,arg2), &u::member_function_name>* = 0);           \
   static yes deduce(...);                                                                                   \
public:                                                                                                      \
   static const bool value = sizeof(yes) == sizeof(deduce(static_cast<base*>(0)));                           \
};                                                                                                           \
                                                                                                             \
template<typename z, typename result, typename arg1, typename arg2, typename arg3>                           \
class trait_name<z, result(arg1,arg2,arg3)>                                                                  \
{                                                                                                            \
   class yes { char m; };                                                                                    \
   class no { yes m[2]; };                                                                                   \
   struct base_mixin                                                                                         \
   {                                                                                                         \
     result member_function_name(arg1,arg2,arg3);                                                            \
   };                                                                                                        \
   struct base : public z, public base_mixin { private: base(); };                                           \
   template <typename u, u t>  class helper{};                                                               \
   template <typename u>                                                                                     \
   static no deduce(u*, helper<result (base_mixin::*)(arg1,arg2,arg3), &u::member_function_name>* = 0);      \
   static yes deduce(...);                                                                                   \
public:                                                                                                      \
   static const bool value = sizeof(yes) == sizeof(deduce(static_cast<base*>(0)));                           \
};                                                                                                           \
                                                                                                             \
template<typename z, typename result, typename arg1, typename arg2, typename arg3, typename arg4>            \
class trait_name<z, result(arg1,arg2,arg3,arg4)>                                                             \
{                                                                                                            \
   class yes { char m; };                                                                                    \
   class no { yes m[2]; };                                                                                   \
   struct base_mixin                                                                                         \
   {                                                                                                         \
     result member_function_name(arg1,arg2,arg3,arg4);                                                       \
   };                                                                                                        \
   struct base : public z, public base_mixin { private: base(); };                                           \
   template <typename u, u t>  class helper{};                                                               \
   template <typename u>                                                                                     \
   static no deduce(u*, helper<result (base_mixin::*)(arg1,arg2,arg3,arg4), &u::member_function_name>* = 0); \
   static yes deduce(...);                                                                                   \
public:                                                                                                      \
   static const bool value = sizeof(yes) == sizeof(deduce(static_cast<base*>(0)));                           \
}                                                                                                           

#define beast_define_is_call_possible(trait_name, member_function_name)                                                 \
struct trait_name##_detail                                                                                              \
{                                                                                                                       \
beast_define_has_member_function(has_member, member_function_name);                                                     \
};                                                                                                                      \
                                                                                                                        \
template <typename dt, typename iscallpossiblesignature>                                                                \
struct trait_name                                                                                                       \
{                                                                                                                       \
private:                                                                                                                \
   typedef std::remove_reference_t <dt> z;                                                                              \
   class yes {};                                                                                                        \
   class no { yes m[2]; };                                                                                              \
   struct derived : public z                                                                                            \
   {                                                                                                                    \
     using z::member_function_name;                                                                                     \
     no member_function_name(...) const;                                                                                \
     private: derived ();                                                                                               \
   };                                                                                                                   \
                                                                                                                        \
   typedef typename beast::is_call_possible_detail::clone_constness<z, derived>::type derived_type;                \
                                                                                                                        \
   template <typename u, typename result>                                                                               \
   struct return_value_check                                                                                            \
   {                                                                                                                    \
     static yes deduce(result);                                                                                         \
     static no deduce(...);                                                                                             \
     static no deduce(no);                                                                                              \
     static no deduce(beast::is_call_possible_detail::void_exp_result<z>);                                         \
   };                                                                                                                   \
                                                                                                                        \
   template <typename u>                                                                                                \
   struct return_value_check<u, void>                                                                                   \
   {                                                                                                                    \
     static yes deduce(...);                                                                                            \
     static no deduce(no);                                                                                              \
   };                                                                                                                   \
                                                                                                                        \
   template <bool has_the_member_of_interest, typename f>                                                               \
   struct impl                                                                                                          \
   {                                                                                                                    \
     static const bool value = false;                                                                                   \
   };                                                                                                                   \
                                                                                                                        \
   template <typename result>                                                                                           \
   struct impl<true, result(void)>                                                                                      \
   {                                                                                                                    \
     static typename beast::is_call_possible_detail::add_reference<derived_type>::type test_me;                    \
                                                                                                                        \
     static const bool value =                                                                                          \
       sizeof(                                                                                                          \
            return_value_check<z, result>::deduce(                                                                      \
             (test_me.member_function_name(), beast::is_call_possible_detail::void_exp_result<z>()))               \
            ) == sizeof(yes);                                                                                           \
   };                                                                                                                   \
                                                                                                                        \
   template <typename result, typename arg>                                                                             \
   struct impl<true, result(arg)>                                                                                       \
   {                                                                                                                    \
     static typename beast::is_call_possible_detail::add_reference<derived_type>::type test_me;                    \
     static typename beast::is_call_possible_detail::add_reference<arg>::type          arg;                        \
                                                                                                                        \
     static const bool value =                                                                                          \
       sizeof(                                                                                                          \
            return_value_check<z, result>::deduce(                                                                      \
             (test_me.member_function_name(arg), beast::is_call_possible_detail::void_exp_result<z>())             \
                         )                                                                                              \
            ) == sizeof(yes);                                                                                           \
   };                                                                                                                   \
                                                                                                                        \
   template <typename result, typename arg1, typename arg2>                                                             \
   struct impl<true, result(arg1,arg2)>                                                                                 \
   {                                                                                                                    \
     static typename beast::is_call_possible_detail::add_reference<derived_type>::type test_me;                    \
     static typename beast::is_call_possible_detail::add_reference<arg1>::type         arg1;                       \
     static typename beast::is_call_possible_detail::add_reference<arg2>::type         arg2;                       \
                                                                                                                        \
     static const bool value =                                                                                          \
       sizeof(                                                                                                          \
            return_value_check<z, result>::deduce(                                                                      \
             (test_me.member_function_name(arg1,arg2), beast::is_call_possible_detail::void_exp_result<z>())       \
                         )                                                                                              \
            ) == sizeof(yes);                                                                                           \
   };                                                                                                                   \
                                                                                                                        \
   template <typename result, typename arg1, typename arg2, typename arg3>                                              \
   struct impl<true, result(arg1,arg2,arg3)>                                                                            \
   {                                                                                                                    \
     static typename beast::is_call_possible_detail::add_reference<derived_type>::type test_me;                    \
     static typename beast::is_call_possible_detail::add_reference<arg1>::type         arg1;                       \
     static typename beast::is_call_possible_detail::add_reference<arg2>::type         arg2;                       \
     static typename beast::is_call_possible_detail::add_reference<arg3>::type         arg3;                       \
                                                                                                                        \
     static const bool value =                                                                                          \
       sizeof(                                                                                                          \
            return_value_check<z, result>::deduce(                                                                      \
             (test_me.member_function_name(arg1,arg2,arg3), beast::is_call_possible_detail::void_exp_result<z>())  \
                         )                                                                                              \
            ) == sizeof(yes);                                                                                           \
   };                                                                                                                   \
                                                                                                                        \
   template <typename result, typename arg1, typename arg2, typename arg3, typename arg4>                               \
   struct impl<true, result(arg1,arg2,arg3,arg4)>                                                                       \
   {                                                                                                                    \
     static typename beast::is_call_possible_detail::add_reference<derived_type>::type test_me;                    \
     static typename beast::is_call_possible_detail::add_reference<arg1>::type         arg1;                       \
     static typename beast::is_call_possible_detail::add_reference<arg2>::type         arg2;                       \
     static typename beast::is_call_possible_detail::add_reference<arg3>::type         arg3;                       \
     static typename beast::is_call_possible_detail::add_reference<arg4>::type         arg4;                       \
                                                                                                                        \
     static const bool value =                                                                                          \
       sizeof(                                                                                                          \
            return_value_check<z, result>::deduce(                                                                      \
             (test_me.member_function_name(arg1,arg2,arg3,arg4),                                                        \
                beast::is_call_possible_detail::void_exp_result<z>())                                              \
                         )                                                                                              \
            ) == sizeof(yes);                                                                                           \
   };                                                                                                                   \
                                                                                                                        \
  public:                                                                                                               \
    static const bool value = impl<trait_name##_detail::template has_member<z,iscallpossiblesignature>::value,          \
        iscallpossiblesignature>::value;                                                                                \
}

} // beast

#endif
