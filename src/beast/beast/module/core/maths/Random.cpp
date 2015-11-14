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

#include <beast/unit_test/suite.h>

namespace beast {

random::random (const std::int64_t seedvalue) noexcept
    : seed (seedvalue)
{
    nextint (); // fixes a bug where the first int is always 0
}

random::random()
    : seed (1)
{
    setseedrandomly();
}

random::~random() noexcept
{
}

void random::setseed (const std::int64_t newseed) noexcept
{
    seed = newseed;

    nextint (); // fixes a bug where the first int is always 0
}

void random::combineseed (const std::int64_t seedvalue) noexcept
{
    seed ^= nextint64() ^ seedvalue;
}

void random::setseedrandomly()
{
    static std::int64_t globalseed = 0;

    combineseed (globalseed ^ (std::int64_t) (std::intptr_t) this);
    combineseed (time::getmillisecondcounter());
    combineseed (time::gethighresolutionticks());
    combineseed (time::gethighresolutiontickspersecond());
    combineseed (time::currenttimemillis());
    globalseed ^= seed;

    nextint (); // fixes a bug where the first int is always 0
}

random& random::getsystemrandom() noexcept
{
    static random sysrand;
    return sysrand;
}

//==============================================================================
int random::nextint() noexcept
{
    seed = (seed * 0x5deece66dll + 11) & 0xffffffffffffull;

    return (int) (seed >> 16);
}

int random::nextint (const int maxvalue) noexcept
{
    bassert (maxvalue > 0);
    return (int) ((((unsigned int) nextint()) * (std::uint64_t) maxvalue) >> 32);
}

std::int64_t random::nextint64() noexcept
{
    return (((std::int64_t) nextint()) << 32) | (std::int64_t) (std::uint64_t) (std::uint32_t) nextint();
}

bool random::nextbool() noexcept
{
    return (nextint() & 0x40000000) != 0;
}

float random::nextfloat() noexcept
{
    return static_cast <std::uint32_t> (nextint()) / (float) 0xffffffff;
}

double random::nextdouble() noexcept
{
    return static_cast <std::uint32_t> (nextint()) / (double) 0xffffffff;
}

void random::fillbitsrandomly (void* const buffer, size_t bytes)
{
    int* d = static_cast<int*> (buffer);

    for (; bytes >= sizeof (int); bytes -= sizeof (int))
        *d++ = nextint();

    if (bytes > 0)
    {
        const int lastbytes = nextint();
        memcpy (d, &lastbytes, bytes);
    }
}

//==============================================================================

class random_test  : public unit_test::suite
{
public:
    void run()
    {
        for (int j = 10; --j >= 0;)
        {
            random r;
            r.setseedrandomly();

            for (int i = 20; --i >= 0;)
            {
                expect (r.nextdouble() >= 0.0 && r.nextdouble() < 1.0);
                expect (r.nextfloat() >= 0.0f && r.nextfloat() < 1.0f);
                expect (r.nextint (5) >= 0 && r.nextint (5) < 5);
                expect (r.nextint (1) == 0);

                int n = r.nextint (50) + 1;
                expect (r.nextint (n) >= 0 && r.nextint (n) < n);

                n = r.nextint (0x7ffffffe) + 1;
                expect (r.nextint (n) >= 0 && r.nextint (n) < n);
            }
        }
    }
};

beast_define_testsuite(random,beast_core,beast);

} // beast
