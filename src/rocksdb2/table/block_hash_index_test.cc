// copyright (c) 2013, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#include <map>
#include <memory>
#include <vector>

#include "rocksdb/comparator.h"
#include "rocksdb/iterator.h"
#include "rocksdb/slice_transform.h"
#include "table/block_hash_index.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

typedef std::map<std::string, std::string> data;

class mapiterator : public iterator {
 public:
  explicit mapiterator(const data& data) : data_(data), pos_(data_.end()) {}

  virtual bool valid() const { return pos_ != data_.end(); }

  virtual void seektofirst() { pos_ = data_.begin(); }

  virtual void seektolast() {
    pos_ = data_.end();
    --pos_;
  }

  virtual void seek(const slice& target) {
    pos_ = data_.find(target.tostring());
  }

  virtual void next() { ++pos_; }

  virtual void prev() { --pos_; }

  virtual slice key() const { return pos_->first; }

  virtual slice value() const { return pos_->second; }

  virtual status status() const { return status::ok(); }

 private:
  const data& data_;
  data::const_iterator pos_;
};

class blocktest {};

test(blocktest, basictest) {
  const size_t keys_per_block = 4;
  const size_t prefix_size = 2;
  std::vector<std::string> keys = {/* block 1 */
                                   "0101", "0102", "0103", "0201",
                                   /* block 2 */
                                   "0202", "0203", "0301", "0401",
                                   /* block 3 */
                                   "0501", "0601", "0701", "0801",
                                   /* block 4 */
                                   "0802", "0803", "0804", "0805",
                                   /* block 5 */
                                   "0806", "0807", "0808", "0809", };

  data data_entries;
  for (const auto key : keys) {
    data_entries.insert({key, key});
  }

  data index_entries;
  for (size_t i = 3; i < keys.size(); i += keys_per_block) {
    // simply ignore the value part
    index_entries.insert({keys[i], ""});
  }

  mapiterator data_iter(data_entries);
  mapiterator index_iter(index_entries);

  auto prefix_extractor = newfixedprefixtransform(prefix_size);
  std::unique_ptr<blockhashindex> block_hash_index(createblockhashindexonthefly(
      &index_iter, &data_iter, index_entries.size(), bytewisecomparator(),
      prefix_extractor));

  std::map<std::string, blockhashindex::restartindex> expected = {
      {"01xx", blockhashindex::restartindex(0, 1)},
      {"02yy", blockhashindex::restartindex(0, 2)},
      {"03zz", blockhashindex::restartindex(1, 1)},
      {"04pp", blockhashindex::restartindex(1, 1)},
      {"05ww", blockhashindex::restartindex(2, 1)},
      {"06xx", blockhashindex::restartindex(2, 1)},
      {"07pp", blockhashindex::restartindex(2, 1)},
      {"08xz", blockhashindex::restartindex(2, 3)}, };

  const blockhashindex::restartindex* index = nullptr;
  // search existed prefixes
  for (const auto& item : expected) {
    index = block_hash_index->getrestartindex(item.first);
    assert_true(index != nullptr);
    assert_eq(item.second.first_index, index->first_index);
    assert_eq(item.second.num_blocks, index->num_blocks);
  }

  // search non exist prefixes
  assert_true(!block_hash_index->getrestartindex("00xx"));
  assert_true(!block_hash_index->getrestartindex("10yy"));
  assert_true(!block_hash_index->getrestartindex("20zz"));

  delete prefix_extractor;
}

}  // namespace rocksdb

int main(int argc, char** argv) { return rocksdb::test::runalltests(); }
