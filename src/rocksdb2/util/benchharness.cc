//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// this code is derived from benchmark.cpp implemented in folly, the opensourced
// facebook c++ library available at https://github.com/facebook/folly
// the code has removed any dependence on other folly and boost libraries

#include "util/benchharness.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using std::function;
using std::get;
using std::make_pair;
using std::max;
using std::min;
using std::pair;
using std::sort;
using std::string;
using std::tuple;
using std::vector;

define_bool(benchmark, false, "run benchmarks.");

define_int64(bm_min_usec, 100,
             "minimum # of microseconds we'll accept for each benchmark.");

define_int64(bm_min_iters, 1,
             "minimum # of iterations we'll try for each benchmark.");

define_int32(bm_max_secs, 1,
             "maximum # of seconds we'll spend on each benchmark.");


namespace rocksdb {
namespace benchmark {

benchmarksuspender::nanosecondsspent benchmarksuspender::nsspent;

typedef function<uint64_t(unsigned int)> benchmarkfun;
static vector<tuple<const char*, const char*, benchmarkfun>> benchmarks;

// add the global baseline
benchmark(globalbenchmarkbaseline) {
  asm volatile("");
}

void detail::addbenchmarkimpl(const char* file, const char* name,
                              benchmarkfun fun) {
  benchmarks.emplace_back(file, name, std::move(fun));
}

/**
 * given a point, gives density at that point as a number 0.0 < x <=
 * 1.0. the result is 1.0 if all samples are equal to where, and
 * decreases near 0 if all points are far away from it. the density is
 * computed with the help of a radial basis function.
 */
static double density(const double * begin, const double *const end,
                      const double where, const double bandwidth) {
  assert(begin < end);
  assert(bandwidth > 0.0);
  double sum = 0.0;
  for (auto i = begin; i < end; i++) {
    auto d = (*i - where) / bandwidth;
    sum += exp(- d * d);
  }
  return sum / (end - begin);
}

/**
 * computes mean and variance for a bunch of data points. note that
 * mean is currently not being used.
 */
static pair<double, double>
meanvariance(const double * begin, const double *const end) {
  assert(begin < end);
  double sum = 0.0, sum2 = 0.0;
  for (auto i = begin; i < end; i++) {
    sum += *i;
    sum2 += *i * *i;
  }
  auto const n = end - begin;
  return make_pair(sum / n, sqrt((sum2 - sum * sum / n) / n));
}

/**
 * computes the mode of a sample set through brute force. assumes
 * input is sorted.
 */
static double mode(const double * begin, const double *const end) {
  assert(begin < end);
  // lower bound and upper bound for result and their respective
  // densities.
  auto
    result = 0.0,
    bestdensity = 0.0;

  // get the variance so we pass it down to density()
  auto const sigma = meanvariance(begin, end).second;
  if (!sigma) {
    // no variance means constant signal
    return *begin;
  }

  for (auto i = begin; i < end; i++) {
    assert(i == begin || *i >= i[-1]);
    auto candidate = density(begin, end, *i, sigma * sqrt(2.0));
    if (candidate > bestdensity) {
      // found a new best
      bestdensity = candidate;
      result = *i;
    } else {
      // density is decreasing... we could break here if we definitely
      // knew this is unimodal.
    }
  }

  return result;
}

/**
 * given a bunch of benchmark samples, estimate the actual run time.
 */
static double estimatetime(double * begin, double * end) {
  assert(begin < end);

  // current state of the art: get the minimum. after some
  // experimentation, it seems taking the minimum is the best.

  return *std::min_element(begin, end);

  // what follows after estimates the time as the mode of the
  // distribution.

  // select the awesomest (i.e. most frequent) result. we do this by
  // sorting and then computing the longest run length.
  sort(begin, end);

  // eliminate outliers. a time much larger than the minimum time is
  // considered an outlier.
  while (end[-1] > 2.0 * *begin) {
    --end;
    if (begin == end) {
//      log(info) << *begin;
    }
    assert(begin < end);
  }

  double result = 0;

  /* code used just for comparison purposes */ {
    unsigned bestfrequency = 0;
    unsigned candidatefrequency = 1;
    double candidatevalue = *begin;
    for (auto current = begin + 1; ; ++current) {
      if (current == end || *current != candidatevalue) {
        // done with the current run, see if it was best
        if (candidatefrequency > bestfrequency) {
          bestfrequency = candidatefrequency;
          result = candidatevalue;
        }
        if (current == end) {
          break;
        }
        // start a new run
        candidatevalue = *current;
        candidatefrequency = 1;
      } else {
        // cool, inside a run, increase the frequency
        ++candidatefrequency;
      }
    }
  }

  result = mode(begin, end);

  return result;
}

static double runbenchmarkgetnsperiteration(const benchmarkfun& fun,
                                            const double globalbaseline) {
  // they key here is accuracy; too low numbers means the accuracy was
  // coarse. we up the ante until we get to at least minnanoseconds
  // timings.
  static const auto minnanoseconds = flags_bm_min_usec * 1000ul;

  // we do measurements in several epochs and take the minimum, to
  // account for jitter.
  static const unsigned int epochs = 1000;
  // we establish a total time budget as we don't want a measurement
  // to take too long. this will curtail the number of actual epochs.
  const uint64_t timebudgetinns = flags_bm_max_secs * 1000000000;
  auto env = env::default();
  uint64_t global = env->nownanos();

  double epochresults[epochs] = { 0 };
  size_t actualepochs = 0;

  for (; actualepochs < epochs; ++actualepochs) {
    for (unsigned int n = flags_bm_min_iters; n < (1ul << 30); n *= 2) {
      auto const nsecs = fun(n);
      if (nsecs < minnanoseconds) {
        continue;
      }
      // we got an accurate enough timing, done. but only save if
      // smaller than the current result.
      epochresults[actualepochs] = max(0.0,
          static_cast<double>(nsecs) / n - globalbaseline);
      // done with the current epoch, we got a meaningful timing.
      break;
    }
    uint64_t now = env->nownanos();
    if ((now - global) >= timebudgetinns) {
      // no more time budget available.
      ++actualepochs;
      break;
    }
  }

  // if the benchmark was basically drowned in baseline noise, it's
  // possible it became negative.
  return max(0.0, estimatetime(epochresults, epochresults + actualepochs));
}

struct scaleinfo {
  double boundary;
  const char* suffix;
};

static const scaleinfo ktimesuffixes[] {
  { 365.25 * 24 * 3600, "years" },
  { 24 * 3600, "days" },
  { 3600, "hr" },
  { 60, "min" },
  { 1, "s" },
  { 1e-3, "ms" },
  { 1e-6, "us" },
  { 1e-9, "ns" },
  { 1e-12, "ps" },
  { 1e-15, "fs" },
  { 0, nullptr },
};

static const scaleinfo kmetricsuffixes[] {
  { 1e24, "y" },  // yotta
  { 1e21, "z" },  // zetta
  { 1e18, "x" },  // "exa" written with suffix 'x' so as to not create
                  //   confusion with scientific notation
  { 1e15, "p" },  // peta
  { 1e12, "t" },  // terra
  { 1e9, "g" },   // giga
  { 1e6, "m" },   // mega
  { 1e3, "k" },   // kilo
  { 1, "" },
  { 1e-3, "m" },  // milli
  { 1e-6, "u" },  // micro
  { 1e-9, "n" },  // nano
  { 1e-12, "p" },  // pico
  { 1e-15, "f" },  // femto
  { 1e-18, "a" },  // atto
  { 1e-21, "z" },  // zepto
  { 1e-24, "y" },  // yocto
  { 0, nullptr },
};

static string humanreadable(double n, unsigned int decimals,
                            const scaleinfo* scales) {
  if (std::isinf(n) || std::isnan(n)) {
    return std::to_string(n);
  }

  const double absvalue = fabs(n);
  const scaleinfo* scale = scales;
  while (absvalue < scale[0].boundary && scale[1].suffix != nullptr) {
    ++scale;
  }

  const double scaledvalue = n / scale->boundary;
  char a[80];
  snprintf(a, sizeof(a), "%.*f%s", decimals, scaledvalue, scale->suffix);
  return a;
}

static string readabletime(double n, unsigned int decimals) {
  return humanreadable(n, decimals, ktimesuffixes);
}

static string metricreadable(double n, unsigned int decimals) {
  return humanreadable(n, decimals, kmetricsuffixes);
}

static void printbenchmarkresultsastable(
  const vector<tuple<const char*, const char*, double> >& data) {
  // width available
  static const uint columns = 76;

  // compute the longest benchmark name
  size_t longestname = 0;
  for (size_t i = 1; i < benchmarks.size(); i++) {
    longestname = max(longestname, strlen(get<1>(benchmarks[i])));
  }

  // print a horizontal rule
  auto separator = [&](char pad) {
    puts(string(columns, pad).c_str());
  };

  // print header for a file
  auto header = [&](const char* file) {
    separator('=');
    printf("%-*srelative  time/iter  iters/s\n",
           columns - 28, file);
    separator('=');
  };

  double baselinensperiter = std::numeric_limits<double>::max();
  const char* lastfile = "";

  for (auto& datum : data) {
    auto file = get<0>(datum);
    if (strcmp(file, lastfile)) {
      // new file starting
      header(file);
      lastfile = file;
    }

    string s = get<1>(datum);
    if (s == "-") {
      separator('-');
      continue;
    }
    bool usebaseline /* = void */;
    if (s[0] == '%') {
      s.erase(0, 1);
      usebaseline = true;
    } else {
      baselinensperiter = get<2>(datum);
      usebaseline = false;
    }
    s.resize(columns - 29, ' ');
    auto nsperiter = get<2>(datum);
    auto secperiter = nsperiter / 1e9;
    auto iterspersec = 1 / secperiter;
    if (!usebaseline) {
      // print without baseline
      printf("%*s           %9s  %7s\n",
             static_cast<int>(s.size()), s.c_str(),
             readabletime(secperiter, 2).c_str(),
             metricreadable(iterspersec, 2).c_str());
    } else {
      // print with baseline
      auto rel = baselinensperiter / nsperiter * 100.0;
      printf("%*s %7.2f%%  %9s  %7s\n",
             static_cast<int>(s.size()), s.c_str(),
             rel,
             readabletime(secperiter, 2).c_str(),
             metricreadable(iterspersec, 2).c_str());
    }
  }
  separator('=');
}

void runbenchmarks() {
  assert_true(!benchmarks.empty());

  vector<tuple<const char*, const char*, double>> results;
  results.reserve(benchmarks.size() - 1);

  // please keep quiet. measurements in progress.

  auto const globalbaseline = runbenchmarkgetnsperiteration(
    get<2>(benchmarks.front()), 0);
  for (size_t i = 1; i < benchmarks.size(); i++) {
    double elapsed = 0.0;
    if (strcmp(get<1>(benchmarks[i]), "-") != 0) {  // skip separators
      elapsed = runbenchmarkgetnsperiteration(get<2>(benchmarks[i]),
                                              globalbaseline);
    }
    results.emplace_back(get<0>(benchmarks[i]),
                         get<1>(benchmarks[i]), elapsed);
  }

  // please make noise. measurements done.

  printbenchmarkresultsastable(results);
}

}  // namespace benchmark
}  // namespace rocksdb
