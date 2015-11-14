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

#if beast_include_beastconfig
#include <beastconfig.h>
#endif

#include <beast/cxx14/utility.h>

#include <beast/unit_test/suite.h>

namespace beast {
namespace asio {

class integer_sequence_test : public unit_test::suite
{
public:
    template <class atcontainer, class t, t... i>
    static
    auto
    extract (atcontainer const& t,
        std::integer_sequence <t, i...>) ->
            decltype (std::make_tuple (std::get <i> (t)...))
    {
        return std::make_tuple (std::get <i> (t)...);
    }

    void run()
    {
        // code from
        // http://llvm.org/svn/llvm-project/libcxx/trunk/test/utilities/intseq/intseq.general/integer_seq.pass.cpp

        //  make a couple of sequences
        using int3    = std::make_integer_sequence<int, 3>;     // generates int:    0,1,2
        using size7   = std::make_integer_sequence<size_t, 7>;  // generates size_t: 0,1,2,3,4,5,6
        using size4   = std::make_index_sequence<4>;            // generates size_t: 0,1,2,3
        using size2   = std::index_sequence_for<int, size_t>;   // generates size_t: 0,1
        using intmix  = std::integer_sequence<int, 9, 8, 7, 2>; // generates int:    9,8,7,2
        using sizemix = std::index_sequence<1, 1, 2, 3, 5>;     // generates size_t: 1,1,2,3,5
    
        // make sure they're what we expect
        static_assert ( std::is_same <int3::value_type, int>::value, "int3 type wrong" );
        static_assert ( int3::static_size == 3, "int3 size wrong" );
    
        static_assert ( std::is_same <size7::value_type, size_t>::value, "size7 type wrong" );
        static_assert ( size7::static_size == 7, "size7 size wrong" );
    
        static_assert ( std::is_same <size4::value_type, size_t>::value, "size4 type wrong" );
        static_assert ( size4::static_size == 4, "size4 size wrong" );
    
        static_assert ( std::is_same <size2::value_type, size_t>::value, "size2 type wrong" );
        static_assert ( size2::static_size == 2, "size2 size wrong" );
    
        static_assert ( std::is_same <intmix::value_type, int>::value, "intmix type wrong" );
        static_assert ( intmix::static_size == 4, "intmix size wrong" );

        static_assert ( std::is_same <sizemix::value_type, size_t>::value, "sizemix type wrong" );
        static_assert ( sizemix::static_size == 5, "sizemix size wrong" );

        auto tup = std::make_tuple ( 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 );
    
        //  use them
        auto t3 = extract ( tup, int3() );
        static_assert ( std::tuple_size<decltype(t3)>::value == int3::static_size, "t3 size wrong");
        expect ( t3 == std::make_tuple ( 10, 11, 12 ));

        auto t7 = extract ( tup, size7 ());
        static_assert ( std::tuple_size<decltype(t7)>::value == size7::static_size, "t7 size wrong");
        expect ( t7 == std::make_tuple ( 10, 11, 12, 13, 14, 15, 16 ));

        auto t4 = extract ( tup, size4 ());
        static_assert ( std::tuple_size<decltype(t4)>::value == size4::static_size, "t4 size wrong");
        expect ( t4 == std::make_tuple ( 10, 11, 12, 13 ));

        auto t2 = extract ( tup, size2 ());
        static_assert ( std::tuple_size<decltype(t2)>::value == size2::static_size, "t2 size wrong");
        expect ( t2 == std::make_tuple ( 10, 11 ));

        auto tintmix = extract ( tup, intmix ());
        static_assert ( std::tuple_size<decltype(tintmix)>::value == intmix::static_size, "tintmix size wrong");
        expect ( tintmix == std::make_tuple ( 19, 18, 17, 12 ));

        auto tsizemix = extract ( tup, sizemix ());
        static_assert ( std::tuple_size<decltype(tsizemix)>::value == sizemix::static_size, "tsizemix size wrong");
        expect ( tsizemix == std::make_tuple ( 11, 11, 12, 13, 15 ));
        pass();
    }
};

beast_define_testsuite(integer_sequence,cxx14,beast);

}
}
