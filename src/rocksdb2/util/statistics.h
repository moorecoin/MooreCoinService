//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once
#include "rocksdb/statistics.h"

#include <vector>
#include <atomic>
#include <string>

#include "util/histogram.h"
#include "util/mutexlock.h"
#include "port/likely.h"


namespace rocksdb {

enum tickersinternal : uint32_t {
  internal_ticker_enum_start = ticker_enum_max,
  internal_ticker_enum_max
};

enum histogramsinternal : uint32_t {
  internal_histogram_start = histogram_enum_max,
  internal_histogram_enum_max
};


class statisticsimpl : public statistics {
 public:
  statisticsimpl(std::shared_ptr<statistics> stats,
                 bool enable_internal_stats);
  virtual ~statisticsimpl();

  virtual uint64_t gettickercount(uint32_t ticker_type) const override;
  virtual void histogramdata(uint32_t histogram_type,
                             histogramdata* const data) const override;

  virtual void settickercount(uint32_t ticker_type, uint64_t count) override;
  virtual void recordtick(uint32_t ticker_type, uint64_t count) override;
  virtual void measuretime(uint32_t histogram_type, uint64_t value) override;

  virtual std::string tostring() const override;
  virtual bool histenabledfortype(uint32_t type) const override;

 private:
  std::shared_ptr<statistics> stats_shared_;
  statistics* stats_;
  bool enable_internal_stats_;

  struct ticker {
    ticker() : value(uint_fast64_t()) {}

    std::atomic_uint_fast64_t value;
    // pad the structure to make it size of 64 bytes. a plain array of
    // std::atomic_uint_fast64_t results in huge performance degradataion
    // due to false sharing.
    char padding[64 - sizeof(std::atomic_uint_fast64_t)];
  };

  ticker tickers_[internal_ticker_enum_max] __attribute__((aligned(64)));
  histogramimpl histograms_[internal_histogram_enum_max]
      __attribute__((aligned(64)));
};

// utility functions
inline void measuretime(statistics* statistics, uint32_t histogram_type,
                        uint64_t value) {
  if (statistics) {
    statistics->measuretime(histogram_type, value);
  }
}

inline void recordtick(statistics* statistics, uint32_t ticker_type,
                       uint64_t count = 1) {
  if (statistics) {
    statistics->recordtick(ticker_type, count);
  }
}

inline void settickercount(statistics* statistics, uint32_t ticker_type,
                           uint64_t count) {
  if (statistics) {
    statistics->settickercount(ticker_type, count);
  }
}

}
