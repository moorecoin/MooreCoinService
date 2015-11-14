//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// this code is derived from benchmark.h implemented in folly, the opensourced
// facebook c++ library available at https://github.com/facebook/folly
// the code has removed any dependence on other folly and boost libraries

#pragma once

#include <gflags/gflags.h>

#include <cassert>
#include <functional>
#include <limits>

#include "util/testharness.h"
#include "rocksdb/env.h"

namespace rocksdb {
namespace benchmark {

/**
 * runs all benchmarks defined. usually put in main().
 */
void runbenchmarks();

namespace detail {

/**
 * adds a benchmark wrapped in a std::function. only used
 * internally. pass by value is intentional.
 */
void addbenchmarkimpl(const char* file,
                      const char* name,
                      std::function<uint64_t(unsigned int)>);

}  // namespace detail


/**
 * supporting type for benchmark_suspend defined below.
 */
struct benchmarksuspender {
  benchmarksuspender() { start_ = env::default()->nownanos(); }

  benchmarksuspender(const benchmarksuspender&) = delete;
  benchmarksuspender(benchmarksuspender && rhs) {
    start_ = rhs.start_;
    rhs.start_ = 0;
  }

  benchmarksuspender& operator=(const benchmarksuspender &) = delete;
  benchmarksuspender& operator=(benchmarksuspender && rhs) {
    if (start_ > 0) {
      tally();
    }
    start_ = rhs.start_;
    rhs.start_ = 0;
    return *this;
  }

  ~benchmarksuspender() {
    if (start_ > 0) {
      tally();
    }
  }

  void dismiss() {
    assert(start_ > 0);
    tally();
    start_ = 0;
  }

  void rehire() { start_ = env::default()->nownanos(); }

  /**
   * this helps the macro definition. to get around the dangers of
   * operator bool, returns a pointer to member (which allows no
   * arithmetic).
   */
  /* implicit */
  operator int benchmarksuspender::*() const { return nullptr; }

  /**
   * accumulates nanoseconds spent outside benchmark.
   */
  typedef uint64_t nanosecondsspent;
  static nanosecondsspent nsspent;

 private:
  void tally() {
    uint64_t end = env::default()->nownanos();
    nsspent += start_ - end;
    start_ = end;
  }

  uint64_t start_;
};

/**
 * adds a benchmark. usually not called directly but instead through
 * the macro benchmark defined below. the lambda function involved
 * must take exactly one parameter of type unsigned, and the benchmark
 * uses it with counter semantics (iteration occurs inside the
 * function).
 */
template <typename lambda>
void
addbenchmark_n(const char* file, const char* name, lambda&& lambda) {
  auto execute = [=](unsigned int times) -> uint64_t {
    benchmarksuspender::nsspent = 0;
    uint64_t start, end;
    auto env = env::default();

    // core measurement starts
    start = env->nownanos();
    lambda(times);
    end = env->nownanos();
    // core measurement ends
    return (end - start) - benchmarksuspender::nsspent;
  };

  detail::addbenchmarkimpl(file, name,
                           std::function<uint64_t(unsigned int)>(execute));
}

/**
 * adds a benchmark. usually not called directly but instead through
 * the macro benchmark defined below. the lambda function involved
 * must take zero parameters, and the benchmark calls it repeatedly
 * (iteration occurs outside the function).
 */
template <typename lambda>
void
addbenchmark(const char* file, const char* name, lambda&& lambda) {
  addbenchmark_n(file, name, [=](unsigned int times) {
      while (times-- > 0) {
        lambda();
      }
    });
}

}  // namespace benchmark
}  // namespace rocksdb

/**
 * fb_one_or_none(hello, world) expands to hello and
 * fb_one_or_none(hello) expands to nothing. this macro is used to
 * insert or eliminate text based on the presence of another argument.
 */
#define fb_one_or_none(a, ...) fb_third(a, ## __va_args__, a)
#define fb_third(a, b, ...) __va_args__

#define fb_concatenate_impl(s1, s2) s1##s2
#define fb_concatenate(s1, s2) fb_concatenate_impl(s1, s2)

#define fb_anonymous_variable(str) fb_concatenate(str, __line__)

#define fb_stringize(x) #x

/**
 * introduces a benchmark function. used internally, see benchmark and
 * friends below.
 */
#define benchmark_impl_n(funname, stringname, paramtype, paramname)     \
  static void funname(paramtype);                                       \
  static bool fb_anonymous_variable(rocksdbbenchmarkunused) = (         \
    ::rocksdb::benchmark::addbenchmark_n(__file__, stringname,          \
      [](paramtype paramname) { funname(paramname); }),                 \
    true);                                                              \
  static void funname(paramtype paramname)

#define benchmark_impl(funname, stringname)                             \
  static void funname();                                                \
  static bool fb_anonymous_variable(rocksdbbenchmarkunused) = (         \
    ::rocksdb::benchmark::addbenchmark(__file__, stringname,            \
      []() { funname(); }),                                             \
    true);                                                              \
  static void funname()

/**
 * introduces a benchmark function. use with either one one or two
 * arguments. the first is the name of the benchmark. use something
 * descriptive, such as insertvectorbegin. the second argument may be
 * missing, or could be a symbolic counter. the counter dictates how
 * many internal iteration the benchmark does. example:
 *
 * benchmark(vectorpushback) {
 *   vector<int> v;
 *   v.push_back(42);
 * }
 *
 * benchmark_n(insertvectorbegin, n) {
 *   vector<int> v;
 *   for_each_range (i, 0, n) {
 *     v.insert(v.begin(), 42);
 *   }
 * }
 */
#define benchmark_n(name, ...)                                  \
  benchmark_impl_n(                                             \
    name,                                                       \
    fb_stringize(name),                                         \
    fb_one_or_none(unsigned, ## __va_args__),                   \
    __va_args__)

#define benchmark(name)                                         \
  benchmark_impl(                                               \
    name,                                                       \
    fb_stringize(name))

/**
 * defines a benchmark that passes a parameter to another one. this is
 * common for benchmarks that need a "problem size" in addition to
 * "number of iterations". consider:
 *
 * void pushback(uint n, size_t initialsize) {
 *   vector<int> v;
 *   benchmark_suspend {
 *     v.resize(initialsize);
 *   }
 *   for_each_range (i, 0, n) {
 *    v.push_back(i);
 *   }
 * }
 * benchmark_param(pushback, 0)
 * benchmark_param(pushback, 1000)
 * benchmark_param(pushback, 1000000)
 *
 * the benchmark above estimates the speed of push_back at different
 * initial sizes of the vector. the framework will pass 0, 1000, and
 * 1000000 for initialsize, and the iteration count for n.
 */
#define benchmark_param(name, param)                                    \
  benchmark_named_param(name, param, param)

/*
 * like benchmark_param(), but allows a custom name to be specified for each
 * parameter, rather than using the parameter value.
 *
 * useful when the parameter value is not a valid token for string pasting,
 * of when you want to specify multiple parameter arguments.
 *
 * for example:
 *
 * void addvalue(uint n, int64_t bucketsize, int64_t min, int64_t max) {
 *   histogram<int64_t> hist(bucketsize, min, max);
 *   int64_t num = min;
 *   for_each_range (i, 0, n) {
 *     hist.addvalue(num);
 *     ++num;
 *     if (num > max) { num = min; }
 *   }
 * }
 *
 * benchmark_named_param(addvalue, 0_to_100, 1, 0, 100)
 * benchmark_named_param(addvalue, 0_to_1000, 10, 0, 1000)
 * benchmark_named_param(addvalue, 5k_to_20k, 250, 5000, 20000)
 */
#define benchmark_named_param(name, param_name, ...)                    \
  benchmark_impl(                                                       \
      fb_concatenate(name, fb_concatenate(_, param_name)),              \
      fb_stringize(name) "(" fb_stringize(param_name) ")") {            \
    name(__va_args__);                                                  \
  }

#define benchmark_named_param_n(name, param_name, ...)                  \
  benchmark_impl_n(                                                     \
      fb_concatenate(name, fb_concatenate(_, param_name)),              \
      fb_stringize(name) "(" fb_stringize(param_name) ")",              \
      unsigned,                                                         \
      iters) {                                                          \
    name(iters, ## __va_args__);                                        \
  }

/**
 * just like benchmark, but prints the time relative to a
 * baseline. the baseline is the most recent benchmark() seen in
 * lexical order. example:
 *
 * // this is the baseline
 * benchmark_n(insertvectorbegin, n) {
 *   vector<int> v;
 *   for_each_range (i, 0, n) {
 *     v.insert(v.begin(), 42);
 *   }
 * }
 *
 * benchmark_relative_n(insertlistbegin, n) {
 *   list<int> s;
 *   for_each_range (i, 0, n) {
 *     s.insert(s.begin(), 42);
 *   }
 * }
 *
 * any number of relative benchmark can be associated with a
 * baseline. another benchmark() occurrence effectively establishes a
 * new baseline.
 */
#define benchmark_relative_n(name, ...)                         \
  benchmark_impl_n(                                             \
    name,                                                       \
    "%" fb_stringize(name),                                     \
    fb_one_or_none(unsigned, ## __va_args__),                   \
    __va_args__)

#define benchmark_relative(name)                                \
  benchmark_impl(                                               \
    name,                                                       \
    "%" fb_stringize(name))

/**
 * a combination of benchmark_relative and benchmark_param.
 */
#define benchmark_relative_param(name, param)                   \
  benchmark_relative_named_param(name, param, param)

/**
 * a combination of benchmark_relative and benchmark_named_param.
 */
#define benchmark_relative_named_param(name, param_name, ...)           \
  benchmark_impl_n(                                                     \
      fb_concatenate(name, fb_concatenate(_, param_name)),              \
      "%" fb_stringize(name) "(" fb_stringize(param_name) ")",          \
      unsigned,                                                         \
      iters) {                                                          \
    name(iters, ## __va_args__);                                        \
  }

/**
 * draws a line of dashes.
 */
#define benchmark_draw_line()                                       \
  static bool fb_anonymous_variable(rocksdbbenchmarkunused) = (     \
    ::rocksdb::benchmark::addbenchmark(__file__, "-", []() { }),    \
    true);

/**
 * allows execution of code that doesn't count torward the benchmark's
 * time budget. example:
 *
 * benchmark_start_group(insertvectorbegin, n) {
 *   vector<int> v;
 *   benchmark_suspend {
 *     v.reserve(n);
 *   }
 *   for_each_range (i, 0, n) {
 *     v.insert(v.begin(), 42);
 *   }
 * }
 */
#define benchmark_suspend                               \
  if (auto fb_anonymous_variable(benchmark_suspend) =   \
      ::rocksdb::benchmark::benchmarksuspender()) {}    \
  else
