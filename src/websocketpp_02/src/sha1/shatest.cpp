/*
 *  shatest.cpp
 *
 *  copyright (c) 1998, 2009
 *  paul e. jones <paulej@packetizer.com>
 *  all rights reserved
 *
 *****************************************************************************
 *  $id: shatest.cpp 12 2009-06-22 19:34:25z paulej $
 *****************************************************************************
 *
 *  description:
 *      this file will exercise the sha1 class and perform the three
 *      tests documented in fips pub 180-1.
 *
 *  portability issues:
 *      none.
 *
 */

#include <iostream>
#include "sha1.h"

using namespace std;

/*
 *  define patterns for testing
 */
#define testa   "abc"
#define testb   "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"

/*
 *  function prototype
 */
void displaymessagedigest(unsigned *message_digest);

/*  
 *  main
 *
 *  description:
 *      this is the entry point for the program
 *
 *  parameters:
 *      none.
 *
 *  returns:
 *      nothing.
 *
 *  comments:
 *
 */
int main()
{
    sha1        sha;
    unsigned    message_digest[5];

    /*
     *  perform test a
     */
    cout << endl << "test a: 'abc'" << endl;

    sha.reset();
    sha << testa;

    if (!sha.result(message_digest))
    {
        cerr << "error-- could not compute message digest" << endl;
    }
    else
    {
        displaymessagedigest(message_digest);
        cout << "should match:" << endl;
        cout << '\t' << "a9993e36 4706816a ba3e2571 7850c26c 9cd0d89d" << endl;
    }

    /*
     *  perform test b
     */
    cout << endl << "test b: " << testb << endl;

    sha.reset();
    sha << testb;

    if (!sha.result(message_digest))
    {
        cerr << "error-- could not compute message digest" << endl;
    }
    else
    {
        displaymessagedigest(message_digest);
        cout << "should match:" << endl;
        cout << '\t' << "84983e44 1c3bd26e baae4aa1 f95129e5 e54670f1" << endl;
    }

    /*
     *  perform test c
     */
    cout << endl << "test c: one million 'a' characters" << endl;

    sha.reset();
    for(int i = 1; i <= 1000000; i++) sha.input('a');

    if (!sha.result(message_digest))
    {
        cerr << "error-- could not compute message digest" << endl;
    }
    else
    {
        displaymessagedigest(message_digest);
        cout << "should match:" << endl;
        cout << '\t' << "34aa973c d4c4daa4 f61eeb2b dbad2731 6534016f" << endl;
    }

    return 0;
}

/*  
 *  displaymessagedigest
 *
 *  description:
 *      display message digest array
 *
 *  parameters:
 *      none.
 *
 *  returns:
 *      nothing.
 *
 *  comments:
 *
 */
void displaymessagedigest(unsigned *message_digest)
{
    ios::fmtflags   flags;

    cout << '\t';

    flags = cout.setf(ios::hex|ios::uppercase,ios::basefield);
    cout.setf(ios::uppercase);

    for(int i = 0; i < 5 ; i++)
    {
        cout << message_digest[i] << ' ';
    }

    cout << endl;

    cout.setf(flags);
}
