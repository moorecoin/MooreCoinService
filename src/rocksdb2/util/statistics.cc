//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include "util/statistics.h"

#define __stdc_format_macros
#include <inttypes.h>
#include "rocksdb/statistics.h"
#include "port/likely.h"
#include <algorithm>
#include <cstdio>

namespace rocksdb {

std::shared_ptr<statistics> createdbstatistics() {
  return std::make_shared<statisticsimpl>(nullptr, false);
}

statisticsimpl::statisticsimpl(
    std::shared_ptr<statistics> stats,
    bool enable_internal_stats)
  : stats_shared_(stats),
    stats_(stats.get()),
    enable_internal_stats_(enable_internal_stats) {
}

statisticsimpl::~statisticsimpl() {}

uint64_t statisticsimpl::gettickercount(uint32_t tickertype) const {
  assert(
    enable_internal_stats_ ?
      tickertype < internal_ticker_enum_max :
      tickertype < ticker_enum_max);
  // return its own ticker version
  return tickers_[tickertype].value;
}

void statisticsimpl::histogramdata(uint32_t histogramtype,
                                   histogramdata* const data) const {
  assert(
    enable_internal_stats_ ?
      histogramtype < internal_ticker_enum_max :
      histogramtype < ticker_enum_max);
  // return its own ticker version
  histograms_[histogramtype].data(data);
}

void statisticsimpl::settickercount(uint32_t tickertype, uint64_t count) {
  assert(
    enable_internal_stats_ ?
      tickertype < internal_ticker_enum_max :
      tickertype < ticker_enum_max);
  if (tickertype < ticker_enum_max || enable_internal_stats_) {
    tickers_[tickertype].value = count;
  }
  if (stats_ && tickertype < ticker_enum_max) {
    stats_->settickercount(tickertype, count);
  }
}

void statisticsimpl::recordtick(uint32_t tickertype, uint64_t count) {
  assert(
    enable_internal_stats_ ?
      tickertype < internal_ticker_enum_max :
      tickertype < ticker_enum_max);
  if (tickertype < ticker_enum_max || enable_internal_stats_) {
    tickers_[tickertype].value += count;
  }
  if (stats_ && tickertype < ticker_enum_max) {
    stats_->recordtick(tickertype, count);
  }
}

void statisticsimpl::measuretime(uint32_t histogramtype, uint64_t value) {
  assert(
    enable_internal_stats_ ?
      histogramtype < internal_histogram_enum_max :
      histogramtype < histogram_enum_max);
  if (histogramtype < histogram_enum_max || enable_internal_stats_) {
    histograms_[histogramtype].add(value);
  }
  if (stats_ && histogramtype < histogram_enum_max) {
    stats_->measuretime(histogramtype, value);
  }
}

namespace {

// a buffer size used for temp string buffers
const int kbuffersize = 200;

} // namespace

std::string statisticsimpl::tostring() const {
  std::string res;
  res.reserve(20000);
  for (const auto& t : tickersnamemap) {
    if (t.first < ticker_enum_max || enable_internal_stats_) {
      char buffer[kbuffersize];
      snprintf(buffer, kbuffersize, "%s count : %" priu64 "\n",
               t.second.c_str(), gettickercount(t.first));
      res.append(buffer);
    }
  }
  for (const auto& h : histogramsnamemap) {
    if (h.first < histogram_enum_max || enable_internal_stats_) {
      char buffer[kbuffersize];
      histogramdata hdata;
      histogramdata(h.first, &hdata);
      snprintf(
          buffer,
          kbuffersize,
          "%s statistics percentiles :=> 50 : %f 95 : %f 99 : %f\n",
          h.second.c_str(),
          hdata.median,
          hdata.percentile95,
          hdata.percentile99);
      res.append(buffer);
    }
  }
  res.shrink_to_fit();
  return res;
}

bool statisticsimpl::histenabledfortype(uint32_t type) const {
  if (likely(!enable_internal_stats_)) {
    return type < histogram_enum_max;
  }
  return true;
}

} // namespace rocksdb
