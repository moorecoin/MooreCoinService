#include <memory>
#include "rocksdb/env.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/slice.h"
#include "util/coding.h"
#include "utilities/merge_operators.h"

using namespace rocksdb;

namespace { // anonymous namespace

// a 'model' merge operator with uint64 addition semantics
// implemented as an associativemergeoperator for simplicity and example.
class uint64addoperator : public associativemergeoperator {
 public:
  virtual bool merge(const slice& key,
                     const slice* existing_value,
                     const slice& value,
                     std::string* new_value,
                     logger* logger) const override {
    uint64_t orig_value = 0;
    if (existing_value){
      orig_value = decodeinteger(*existing_value, logger);
    }
    uint64_t operand = decodeinteger(value, logger);

    assert(new_value);
    new_value->clear();
    putfixed64(new_value, orig_value + operand);

    return true;  // return true always since corruption will be treated as 0
  }

  virtual const char* name() const override {
    return "uint64addoperator";
  }

 private:
  // takes the string and decodes it into a uint64_t
  // on error, prints a message and returns 0
  uint64_t decodeinteger(const slice& value, logger* logger) const {
    uint64_t result = 0;

    if (value.size() == sizeof(uint64_t)) {
      result = decodefixed64(value.data());
    } else if (logger != nullptr) {
      // if value is corrupted, treat it as 0
      log(logger, "uint64 value corruption, size: %zu > %zu",
          value.size(), sizeof(uint64_t));
    }

    return result;
  }

};

}

namespace rocksdb {

std::shared_ptr<mergeoperator> mergeoperators::createuint64addoperator() {
  return std::make_shared<uint64addoperator>();
}

}
