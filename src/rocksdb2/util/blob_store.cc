// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef rocksdb_lite
#include "util/blob_store.h"

namespace rocksdb {

using namespace std;

// blobchunk
bool blobchunk::immediatelybefore(const blobchunk& chunk) const {
  // overlapping!?
  assert(!overlap(chunk));
  // size == 0 is a marker, not a block
  return size != 0 &&
    bucket_id == chunk.bucket_id &&
    offset + size == chunk.offset;
}

bool blobchunk::overlap(const blobchunk &chunk) const {
  return size != 0 && chunk.size != 0 && bucket_id == chunk.bucket_id &&
    ((offset >= chunk.offset && offset < chunk.offset + chunk.size) ||
     (chunk.offset >= offset && chunk.offset < offset + size));
}

// blob
string blob::tostring() const {
  string ret;
  for (auto chunk : chunks) {
    putfixed32(&ret, chunk.bucket_id);
    putfixed32(&ret, chunk.offset);
    putfixed32(&ret, chunk.size);
  }
  return ret;
}

blob::blob(const std::string& blob) {
  for (uint32_t i = 0; i < blob.size(); ) {
    uint32_t t[3] = {0};
    for (int j = 0; j < 3 && i + sizeof(uint32_t) - 1 < blob.size();
                    ++j, i += sizeof(uint32_t)) {
      t[j] = decodefixed32(blob.data() + i);
    }
    chunks.push_back(blobchunk(t[0], t[1], t[2]));
  }
}

// freelist
status freelist::free(const blob& blob) {
  // add it back to the free list
  for (auto chunk : blob.chunks) {
    free_blocks_ += chunk.size;
    if (fifo_free_chunks_.size() &&
        fifo_free_chunks_.back().immediatelybefore(chunk)) {
      fifo_free_chunks_.back().size += chunk.size;
    } else {
      fifo_free_chunks_.push_back(chunk);
    }
  }

  return status::ok();
}

status freelist::allocate(uint32_t blocks, blob* blob) {
  if (free_blocks_ < blocks) {
    return status::incomplete("");
  }

  blob->chunks.clear();
  free_blocks_ -= blocks;

  while (blocks > 0) {
    assert(fifo_free_chunks_.size() > 0);
    auto& front = fifo_free_chunks_.front();
    if (front.size > blocks) {
      blob->chunks.push_back(blobchunk(front.bucket_id, front.offset, blocks));
      front.offset += blocks;
      front.size -= blocks;
      blocks = 0;
    } else {
      blob->chunks.push_back(front);
      blocks -= front.size;
      fifo_free_chunks_.pop_front();
    }
  }
  assert(blocks == 0);

  return status::ok();
}

bool freelist::overlap(const blob &blob) const {
  for (auto chunk : blob.chunks) {
    for (auto itr = fifo_free_chunks_.begin();
         itr != fifo_free_chunks_.end();
         ++itr) {
      if (itr->overlap(chunk)) {
        return true;
      }
    }
  }
  return false;
}

// blobstore
blobstore::blobstore(const string& directory,
                     uint64_t block_size,
                     uint32_t blocks_per_bucket,
                     uint32_t max_buckets,
                     env* env) :
    directory_(directory),
    block_size_(block_size),
    blocks_per_bucket_(blocks_per_bucket),
    env_(env),
    max_buckets_(max_buckets) {
  env_->createdirifmissing(directory_);

  storage_options_.use_mmap_writes = false;
  storage_options_.use_mmap_reads = false;

  buckets_size_ = 0;
  buckets_ = new unique_ptr<randomrwfile>[max_buckets_];

  createnewbucket();
}

blobstore::~blobstore() {
  // todo we don't care about recovery for now
  delete [] buckets_;
}

status blobstore::put(const slice& value, blob* blob) {
  // convert size to number of blocks
  status s = allocate((value.size() + block_size_ - 1) / block_size_, blob);
  if (!s.ok()) {
    return s;
  }
  auto size_left = (uint64_t) value.size();

  uint64_t offset = 0; // in bytes, not blocks
  for (auto chunk : blob->chunks) {
    uint64_t write_size = min(chunk.size * block_size_, size_left);
    assert(chunk.bucket_id < buckets_size_);
    s = buckets_[chunk.bucket_id].get()->write(chunk.offset * block_size_,
                                               slice(value.data() + offset,
                                                     write_size));
    if (!s.ok()) {
      delete(*blob);
      return s;
    }
    offset += write_size;
    size_left -= write_size;
    if (write_size < chunk.size * block_size_) {
      // if we have any space left in the block, fill it up with zeros
      string zero_string(chunk.size * block_size_ - write_size, 0);
      s = buckets_[chunk.bucket_id].get()->write(chunk.offset * block_size_ +
                                                    write_size,
                                                 slice(zero_string));
    }
  }

  if (size_left > 0) {
    delete(*blob);
    return status::corruption("tried to write more data than fits in the blob");
  }

  return status::ok();
}

status blobstore::get(const blob& blob,
                      string* value) const {
  {
    // assert that it doesn't overlap with free list
    // it will get compiled out for release
    mutexlock l(&free_list_mutex_);
    assert(!free_list_.overlap(blob));
  }

  value->resize(blob.size() * block_size_);

  uint64_t offset = 0; // in bytes, not blocks
  for (auto chunk : blob.chunks) {
    slice result;
    assert(chunk.bucket_id < buckets_size_);
    status s;
    s = buckets_[chunk.bucket_id].get()->read(chunk.offset * block_size_,
                                              chunk.size * block_size_,
                                              &result,
                                              &value->at(offset));
    if (!s.ok()) {
      value->clear();
      return s;
    }
    if (result.size() < chunk.size * block_size_) {
      value->clear();
      return status::corruption("could not read in from file");
    }
    offset += chunk.size * block_size_;
  }

  // remove the '\0's at the end of the string
  value->erase(find(value->begin(), value->end(), '\0'), value->end());

  return status::ok();
}

status blobstore::delete(const blob& blob) {
  mutexlock l(&free_list_mutex_);
  return free_list_.free(blob);
}

status blobstore::sync() {
  for (size_t i = 0; i < buckets_size_; ++i) {
    status s = buckets_[i].get()->sync();
    if (!s.ok()) {
      return s;
    }
  }
  return status::ok();
}

status blobstore::allocate(uint32_t blocks, blob* blob) {
  mutexlock l(&free_list_mutex_);
  status s;

  s = free_list_.allocate(blocks, blob);
  if (!s.ok()) {
    s = createnewbucket();
    if (!s.ok()) {
      return s;
    }
    s = free_list_.allocate(blocks, blob);
  }

  return s;
}

// called with free_list_mutex_ held
status blobstore::createnewbucket() {
  mutexlock l(&buckets_mutex_);

  if (buckets_size_ >= max_buckets_) {
    return status::notsupported("max size exceeded\n");
  }

  int new_bucket_id = buckets_size_;

  char fname[200];
  sprintf(fname, "%s/%d.bs", directory_.c_str(), new_bucket_id);

  status s = env_->newrandomrwfile(string(fname),
                                   &buckets_[new_bucket_id],
                                   storage_options_);
  if (!s.ok()) {
    return s;
  }

  // whether allocate succeeds or not, does not affect the overall correctness
  // of this function - calling allocate is really optional
  // (also, tmpfs does not support allocate)
  buckets_[new_bucket_id].get()->allocate(0, block_size_ * blocks_per_bucket_);

  buckets_size_ = new_bucket_id + 1;

  return free_list_.free(blob(new_bucket_id, 0, blocks_per_bucket_));
}

} // namespace rocksdb
#endif  // rocksdb_lite
