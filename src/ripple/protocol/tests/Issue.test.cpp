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
#include <ripple/basics/unorderedcontainers.h>
#include <ripple/protocol/book.h>
#include <ripple/protocol/issue.h>
#include <beast/unit_test/suite.h>
#include <map>
#include <set>
#include <typeinfo>
#include <unordered_set>

#if beast_msvc
# define stl_set_has_emplace 1
#else
# define stl_set_has_emplace 0
#endif

#ifndef ripple_assets_enable_std_hash
# if beast_mac || beast_ios
#  define ripple_assets_enable_std_hash 0
# else
#  define ripple_assets_enable_std_hash 1
# endif
#endif

namespace ripple {

class issue_test : public beast::unit_test::suite
{
public:
    // comparison, hash tests for uint60 (via base_uint)
    template <typename unsigned>
    void testunsigned ()
    {
        unsigned const u1 (1);
        unsigned const u2 (2);
        unsigned const u3 (3);

        expect (u1 != u2);
        expect (u1 <  u2);
        expect (u1 <= u2);
        expect (u2 <= u2);
        expect (u2 == u2);
        expect (u2 >= u2);
        expect (u3 >= u2);
        expect (u3 >  u2);

        std::hash <unsigned> hash;

        expect (hash (u1) == hash (u1));
        expect (hash (u2) == hash (u2));
        expect (hash (u3) == hash (u3));
        expect (hash (u1) != hash (u2));
        expect (hash (u1) != hash (u3));
        expect (hash (u2) != hash (u3));
    }

    //--------------------------------------------------------------------------

    // comparison, hash tests for issuetype
    template <class issue>
    void testissuetype ()
    {
        currency const c1 (1); account const i1 (1);
        currency const c2 (2); account const i2 (2);
        currency const c3 (3); account const i3 (3);

        expect (issue (c1, i1) != issue (c2, i1));
        expect (issue (c1, i1) <  issue (c2, i1));
        expect (issue (c1, i1) <= issue (c2, i1));
        expect (issue (c2, i1) <= issue (c2, i1));
        expect (issue (c2, i1) == issue (c2, i1));
        expect (issue (c2, i1) >= issue (c2, i1));
        expect (issue (c3, i1) >= issue (c2, i1));
        expect (issue (c3, i1) >  issue (c2, i1));
        expect (issue (c1, i1) != issue (c1, i2));
        expect (issue (c1, i1) <  issue (c1, i2));
        expect (issue (c1, i1) <= issue (c1, i2));
        expect (issue (c1, i2) <= issue (c1, i2));
        expect (issue (c1, i2) == issue (c1, i2));
        expect (issue (c1, i2) >= issue (c1, i2));
        expect (issue (c1, i3) >= issue (c1, i2));
        expect (issue (c1, i3) >  issue (c1, i2));

        std::hash <issue> hash;

        expect (hash (issue (c1, i1)) == hash (issue (c1, i1)));
        expect (hash (issue (c1, i2)) == hash (issue (c1, i2)));
        expect (hash (issue (c1, i3)) == hash (issue (c1, i3)));
        expect (hash (issue (c2, i1)) == hash (issue (c2, i1)));
        expect (hash (issue (c2, i2)) == hash (issue (c2, i2)));
        expect (hash (issue (c2, i3)) == hash (issue (c2, i3)));
        expect (hash (issue (c3, i1)) == hash (issue (c3, i1)));
        expect (hash (issue (c3, i2)) == hash (issue (c3, i2)));
        expect (hash (issue (c3, i3)) == hash (issue (c3, i3)));
        expect (hash (issue (c1, i1)) != hash (issue (c1, i2)));
        expect (hash (issue (c1, i1)) != hash (issue (c1, i3)));
        expect (hash (issue (c1, i1)) != hash (issue (c2, i1)));
        expect (hash (issue (c1, i1)) != hash (issue (c2, i2)));
        expect (hash (issue (c1, i1)) != hash (issue (c2, i3)));
        expect (hash (issue (c1, i1)) != hash (issue (c3, i1)));
        expect (hash (issue (c1, i1)) != hash (issue (c3, i2)));
        expect (hash (issue (c1, i1)) != hash (issue (c3, i3)));
    }

    template <class set>
    void testissueset ()
    {
        currency const c1 (1);
        account   const i1 (1);
        currency const c2 (2);
        account   const i2 (2);
        issueref const a1 (c1, i1);
        issueref const a2 (c2, i2);

        {
            set c;

            c.insert (a1);
            if (! expect (c.size () == 1)) return;
            c.insert (a2);
            if (! expect (c.size () == 2)) return;

            if (! expect (c.erase (issue (c1, i2)) == 0)) return;
            if (! expect (c.erase (issue (c1, i1)) == 1)) return;
            if (! expect (c.erase (issue (c2, i2)) == 1)) return;
            if (! expect (c.empty ())) return;
        }

        {
            set c;

            c.insert (a1);
            if (! expect (c.size () == 1)) return;
            c.insert (a2);
            if (! expect (c.size () == 2)) return;

            if (! expect (c.erase (issueref (c1, i2)) == 0)) return;
            if (! expect (c.erase (issueref (c1, i1)) == 1)) return;
            if (! expect (c.erase (issueref (c2, i2)) == 1)) return;
            if (! expect (c.empty ())) return;

    #if stl_set_has_emplace
            c.emplace (c1, i1);
            if (! expect (c.size() == 1)) return;
            c.emplace (c2, i2);
            if (! expect (c.size() == 2)) return;
    #endif
        }
    }

    template <class map>
    void testissuemap ()
    {
        currency const c1 (1);
        account   const i1 (1);
        currency const c2 (2);
        account   const i2 (2);
        issueref const a1 (c1, i1);
        issueref const a2 (c2, i2);

        {
            map c;

            c.insert (std::make_pair (a1, 1));
            if (! expect (c.size () == 1)) return;
            c.insert (std::make_pair (a2, 2));
            if (! expect (c.size () == 2)) return;

            if (! expect (c.erase (issue (c1, i2)) == 0)) return;
            if (! expect (c.erase (issue (c1, i1)) == 1)) return;
            if (! expect (c.erase (issue (c2, i2)) == 1)) return;
            if (! expect (c.empty ())) return;
        }

        {
            map c;

            c.insert (std::make_pair (a1, 1));
            if (! expect (c.size () == 1)) return;
            c.insert (std::make_pair (a2, 2));
            if (! expect (c.size () == 2)) return;

            if (! expect (c.erase (issueref (c1, i2)) == 0)) return;
            if (! expect (c.erase (issueref (c1, i1)) == 1)) return;
            if (! expect (c.erase (issueref (c2, i2)) == 1)) return;
            if (! expect (c.empty ())) return;
        }
    }

    void testissuesets ()
    {
        testcase ("std::set <issue>");
        testissueset <std::set <issue>> ();

        testcase ("std::set <issueref>");
        testissueset <std::set <issueref>> ();

#if ripple_assets_enable_std_hash
        testcase ("std::unordered_set <issue>");
        testissueset <std::unordered_set <issue>> ();

        testcase ("std::unordered_set <issueref>");
        testissueset <std::unordered_set <issueref>> ();
#endif

        testcase ("hash_set <issue>");
        testissueset <hash_set <issue>> ();

        testcase ("hash_set <issueref>");
        testissueset <hash_set <issueref>> ();
    }

    void testissuemaps ()
    {
        testcase ("std::map <issue, int>");
        testissuemap <std::map <issue, int>> ();

        testcase ("std::map <issueref, int>");
        testissuemap <std::map <issueref, int>> ();

#if ripple_assets_enable_std_hash
        testcase ("std::unordered_map <issue, int>");
        testissuemap <std::unordered_map <issue, int>> ();

        testcase ("std::unordered_map <issueref, int>");
        testissuemap <std::unordered_map <issueref, int>> ();

        testcase ("hash_map <issue, int>");
        testissuemap <hash_map <issue, int>> ();

        testcase ("hash_map <issueref, int>");
        testissuemap <hash_map <issueref, int>> ();

#endif
    }

    //--------------------------------------------------------------------------

    // comparison, hash tests for booktype
    template <class book>
    void testbook ()
    {
        currency const c1 (1); account const i1 (1);
        currency const c2 (2); account const i2 (2);
        currency const c3 (3); account const i3 (3);

        issue a1 (c1, i1);
        issue a2 (c1, i2);
        issue a3 (c2, i2);
        issue a4 (c3, i2);

        expect (book (a1, a2) != book (a2, a3));
        expect (book (a1, a2) <  book (a2, a3));
        expect (book (a1, a2) <= book (a2, a3));
        expect (book (a2, a3) <= book (a2, a3));
        expect (book (a2, a3) == book (a2, a3));
        expect (book (a2, a3) >= book (a2, a3));
        expect (book (a3, a4) >= book (a2, a3));
        expect (book (a3, a4) >  book (a2, a3));

        std::hash <book> hash;

//         log << std::hex << hash (book (a1, a2));
//         log << std::hex << hash (book (a1, a2));
//
//         log << std::hex << hash (book (a1, a3));
//         log << std::hex << hash (book (a1, a3));
//
//         log << std::hex << hash (book (a1, a4));
//         log << std::hex << hash (book (a1, a4));
//
//         log << std::hex << hash (book (a2, a3));
//         log << std::hex << hash (book (a2, a3));
//
//         log << std::hex << hash (book (a2, a4));
//         log << std::hex << hash (book (a2, a4));
//
//         log << std::hex << hash (book (a3, a4));
//         log << std::hex << hash (book (a3, a4));

        expect (hash (book (a1, a2)) == hash (book (a1, a2)));
        expect (hash (book (a1, a3)) == hash (book (a1, a3)));
        expect (hash (book (a1, a4)) == hash (book (a1, a4)));
        expect (hash (book (a2, a3)) == hash (book (a2, a3)));
        expect (hash (book (a2, a4)) == hash (book (a2, a4)));
        expect (hash (book (a3, a4)) == hash (book (a3, a4)));

        expect (hash (book (a1, a2)) != hash (book (a1, a3)));
        expect (hash (book (a1, a2)) != hash (book (a1, a4)));
        expect (hash (book (a1, a2)) != hash (book (a2, a3)));
        expect (hash (book (a1, a2)) != hash (book (a2, a4)));
        expect (hash (book (a1, a2)) != hash (book (a3, a4)));
    }

    //--------------------------------------------------------------------------

    template <class set>
    void testbookset ()
    {
        currency const c1 (1);
        account   const i1 (1);
        currency const c2 (2);
        account   const i2 (2);
        issueref const a1 (c1, i1);
        issueref const a2 (c2, i2);
        bookref  const b1 (a1, a2);
        bookref  const b2 (a2, a1);

        {
            set c;

            c.insert (b1);
            if (! expect (c.size () == 1)) return;
            c.insert (b2);
            if (! expect (c.size () == 2)) return;

            if (! expect (c.erase (book (a1, a1)) == 0)) return;
            if (! expect (c.erase (book (a1, a2)) == 1)) return;
            if (! expect (c.erase (book (a2, a1)) == 1)) return;
            if (! expect (c.empty ())) return;
        }

        {
            set c;

            c.insert (b1);
            if (! expect (c.size () == 1)) return;
            c.insert (b2);
            if (! expect (c.size () == 2)) return;

            if (! expect (c.erase (bookref (a1, a1)) == 0)) return;
            if (! expect (c.erase (bookref (a1, a2)) == 1)) return;
            if (! expect (c.erase (bookref (a2, a1)) == 1)) return;
            if (! expect (c.empty ())) return;

    #if stl_set_has_emplace
            c.emplace (a1, a2);
            if (! expect (c.size() == 1)) return;
            c.emplace (a2, a1);
            if (! expect (c.size() == 2)) return;
    #endif
        }
    }

    template <class map>
    void testbookmap ()
    {
        currency const c1 (1);
        account   const i1 (1);
        currency const c2 (2);
        account   const i2 (2);
        issueref const a1 (c1, i1);
        issueref const a2 (c2, i2);
        bookref  const b1 (a1, a2);
        bookref  const b2 (a2, a1);

        //typename map::value_type value_type;
        //std::pair <bookref const, int> value_type;

        {
            map c;

            //c.insert (value_type (b1, 1));
            c.insert (std::make_pair (b1, 1));
            if (! expect (c.size () == 1)) return;
            //c.insert (value_type (b2, 2));
            c.insert (std::make_pair (b2, 1));
            if (! expect (c.size () == 2)) return;

            if (! expect (c.erase (book (a1, a1)) == 0)) return;
            if (! expect (c.erase (book (a1, a2)) == 1)) return;
            if (! expect (c.erase (book (a2, a1)) == 1)) return;
            if (! expect (c.empty ())) return;
        }

        {
            map c;

            //c.insert (value_type (b1, 1));
            c.insert (std::make_pair (b1, 1));
            if (! expect (c.size () == 1)) return;
            //c.insert (value_type (b2, 2));
            c.insert (std::make_pair (b2, 1));
            if (! expect (c.size () == 2)) return;

            if (! expect (c.erase (bookref (a1, a1)) == 0)) return;
            if (! expect (c.erase (bookref (a1, a2)) == 1)) return;
            if (! expect (c.erase (bookref (a2, a1)) == 1)) return;
            if (! expect (c.empty ())) return;
        }
    }

    void testbooksets ()
    {
        testcase ("std::set <book>");
        testbookset <std::set <book>> ();

        testcase ("std::set <bookref>");
        testbookset <std::set <bookref>> ();

#if ripple_assets_enable_std_hash
        testcase ("std::unordered_set <book>");
        testbookset <std::unordered_set <book>> ();

        testcase ("std::unordered_set <bookref>");
        testbookset <std::unordered_set <bookref>> ();
#endif

        testcase ("hash_set <book>");
        testbookset <hash_set <book>> ();

        testcase ("hash_set <bookref>");
        testbookset <hash_set <bookref>> ();
    }

    void testbookmaps ()
    {
        testcase ("std::map <book, int>");
        testbookmap <std::map <book, int>> ();

        testcase ("std::map <bookref, int>");
        testbookmap <std::map <bookref, int>> ();

#if ripple_assets_enable_std_hash
        testcase ("std::unordered_map <book, int>");
        testbookmap <std::unordered_map <book, int>> ();

        testcase ("std::unordered_map <bookref, int>");
        testbookmap <std::unordered_map <bookref, int>> ();

        testcase ("hash_map <book, int>");
        testbookmap <hash_map <book, int>> ();

        testcase ("hash_map <bookref, int>");
        testbookmap <hash_map <bookref, int>> ();
#endif
    }

    //--------------------------------------------------------------------------

    void run()
    {
        testcase ("currency");
        testunsigned <currency> ();

        testcase ("account");
        testunsigned <account> ();

        // ---

        testcase ("issue");
        testissuetype <issue> ();

        testcase ("issueref");
        testissuetype <issueref> ();

        testissuesets ();
        testissuemaps ();

        // ---

        testcase ("book");
        testbook <book> ();

        testcase ("bookref");
        testbook <bookref> ();

        testbooksets ();
        testbookmaps ();
    }
};

beast_define_testsuite(issue,types,ripple);

}
