//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, howard hinnant <howard.hinnant@gmail.com>,
        vinnie falco <vinnie.falco@gmail.com

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

#ifndef beast_container_tests_hash_metrics_h_included
#define beast_container_tests_hash_metrics_h_included

#include <algorithm>
#include <cmath>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace beast {
namespace hash_metrics {

// metrics for measuring the quality of container hash functions

/** returns the fraction of duplicate items in the sequence. */
template <class fwditer>
float
collision_factor (fwditer first, fwditer last)
{
    std::set <typename fwditer::value_type> s (first, last);
    return 1 - static_cast <float>(s.size()) / std::distance (first, last);
}

//------------------------------------------------------------------------------

/** returns the deviation of the sequence from the ideal distribution. */
template <class fwditer>
float
distribution_factor (fwditer first, fwditer last)
{
    typedef typename fwditer::value_type value_type;
    static_assert (std::is_unsigned <value_type>::value, "");

    const unsigned nbits = char_bit * sizeof(std::size_t);
    const unsigned rows = nbits / 4;
    unsigned counts[rows][16] = {};
    std::for_each (first, last, [&](typename fwditer::value_type h)
    {
        std::size_t mask = 0xf;
        for (unsigned i = 0; i < rows; ++i, mask <<= 4)
            counts[i][(h & mask) >> 4*i] += 1;
    });
    float mean_rows[rows] = {0};
    float mean_cols[16] = {0};
    for (unsigned i = 0; i < rows; ++i)
    {
        for (unsigned j = 0; j < 16; ++j)
        {
            mean_rows[i] += counts[i][j];
            mean_cols[j] += counts[i][j];
        }
    }
    for (unsigned i = 0; i < rows; ++i)
        mean_rows[i] /= 16;
    for (unsigned j = 0; j < 16; ++j)
        mean_cols[j] /= rows;
    std::pair<float, float> dev[rows][16];
    for (unsigned i = 0; i < rows; ++i)
    {
        for (unsigned j = 0; j < 16; ++j)
        {
            dev[i][j].first = std::abs(counts[i][j] - mean_rows[i]) / mean_rows[i];
            dev[i][j].second = std::abs(counts[i][j] - mean_cols[j]) / mean_cols[j];
        }
    }
    float max_err = 0;
    for (unsigned i = 0; i < rows; ++i)
    {
        for (unsigned j = 0; j < 16; ++j)
        {
            if (max_err < dev[i][j].first)
                max_err = dev[i][j].first;
            if (max_err < dev[i][j].second)
                max_err = dev[i][j].second;
        }
    }
    return max_err;
}

//------------------------------------------------------------------------------

namespace detail {

template <class t>
inline
t
sqr(t t)
{
    return t*t;
}

double
score (int const* bins, std::size_t const bincount, double const k)
{
    double const n = bincount;
    // compute rms^2 value
    double rms_sq = 0;
    for(std::size_t i = 0; i < bincount; ++i)
        rms_sq += sqr(bins[i]);;
    rms_sq /= n;
    // compute fill factor
    double const f = (sqr(k) - 1) / (n*rms_sq - k);
    // rescale to (0,1) with 0 = good, 1 = bad
    return 1 - (f / n);
}

template <class t>
std::uint32_t
window (t* blob, int start, int count )
{
    std::size_t const len = sizeof(t);
    static_assert((len & 3) == 0, "");
    if(count == 0)
        return 0;
    int const nbits = len * char_bit;
    start %= nbits;
    int ndwords = len / 4;
    std::uint32_t const* k = static_cast <
        std::uint32_t const*>(static_cast<void const*>(blob));
    int c = start & (32-1);
    int d = start / 32;
    if(c == 0)
        return (k[d] & ((1 << count) - 1));
    int ia = (d + 1) % ndwords;
    int ib = (d + 0) % ndwords;
    std::uint32_t a = k[ia];
    std::uint32_t b = k[ib];
    std::uint32_t t = (a << (32-c)) | (b >> c);
    t &= ((1 << count)-1);
    return t;
}

} // detail

/** calculated a windowed metric using bins.
    todo need reference (smhasher?)
*/
template <class fwditer>
double
windowed_score (fwditer first, fwditer last)
{
    auto const size (std::distance (first, last));
    int maxwidth = 20;
    // we need at least 5 keys per bin to reliably test distribution biases
    // down to 1%, so don't bother to test sparser distributions than that
    while (static_cast<double>(size) / (1 << maxwidth) < 5.0)
        maxwidth--;
    double worst = 0;
    std::vector <int> bins (1 << maxwidth);
    int const hashbits = sizeof(std::size_t) * char_bit;
    for (int start = 0; start < hashbits; ++start)
    {
        int width = maxwidth;
        bins.assign (1 << width, 0);
        for (auto iter (first); iter != last; ++iter)
            ++bins[detail::window(&*iter, start, width)];
        // test the distribution, then fold the bins in half,
        // repeat until we're down to 256 bins
        while (bins.size() >= 256)
        {
            double score (detail::score (
                bins.data(), bins.size(), size));
            worst = std::max(score, worst);
            if (--width < 8)
                break;
            for (std::size_t i = 0, j = bins.size() / 2; j < bins.size(); ++i, ++j)
                bins[i] += bins[j];
            bins.resize(bins.size() / 2);
        }
    }
    return worst;
}

} // hash_metrics
} // beast

#endif
