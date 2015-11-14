//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

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

#ifndef beast_random_h_included
#define beast_random_h_included

#include <cstddef>
#include <cstdint>
    
namespace beast {

//==============================================================================
/**
    a random number generator.

    you can create a random object and use it to generate a sequence of random numbers.
*/
class random
{
public:
    //==============================================================================
    /** creates a random object based on a seed value.

        for a given seed value, the subsequent numbers generated by this object
        will be predictable, so a good idea is to set this value based
        on the time, e.g.

        new random (time::currenttimemillis())
    */
    explicit random (std::int64_t seedvalue) noexcept;

    /** creates a random object using a random seed value.
        internally, this calls setseedrandomly() to randomise the seed.
    */
    random();

    /** destructor. */
    ~random() noexcept;

    /** returns the next random 32 bit integer.

        @returns a random integer from the full range 0x80000000 to 0x7fffffff
    */
    int nextint() noexcept;

    /** returns the next random number, limited to a given range.
        the maxvalue parameter may not be negative, or zero.
        @returns a random integer between 0 (inclusive) and maxvalue (exclusive).
    */
    int nextint (int maxvalue) noexcept;

    /** returns the next 64-bit random number.

        @returns a random integer from the full range 0x8000000000000000 to 0x7fffffffffffffff
    */
    std::int64_t nextint64() noexcept;

    /** returns the next random floating-point number.

        @returns a random value in the range 0 to 1.0
    */
    float nextfloat() noexcept;

    /** returns the next random floating-point number.

        @returns a random value in the range 0 to 1.0
    */
    double nextdouble() noexcept;

    /** returns the next random boolean value.
    */
    bool nextbool() noexcept;

    /** fills a block of memory with random values. */
    void fillbitsrandomly (void* buffertofill, size_t sizeinbytes);

    //==============================================================================
    /** resets this random object to a given seed value. */
    void setseed (std::int64_t newseed) noexcept;

    /** merges this object's seed with another value.
        this sets the seed to be a value created by combining the current seed and this
        new value.
    */
    void combineseed (std::int64_t seedvalue) noexcept;

    /** reseeds this generator using a value generated from various semi-random system
        properties like the current time, etc.

        because this function convolves the time with the last seed value, calling
        it repeatedly will increase the randomness of the final result.
    */
    void setseedrandomly();

    /** the overhead of creating a new random object is fairly small, but if you want to avoid
        it, you can call this method to get a global shared random object.

        it's not thread-safe though, so threads should use their own random object, otherwise
        you run the risk of your random numbers becoming.. erm.. randomly corrupted..
    */
    static random& getsystemrandom() noexcept;

private:
    //==============================================================================
    std::int64_t seed;
};

} // beast

#endif   // beast_random_h_included