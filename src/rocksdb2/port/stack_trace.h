//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once
namespace rocksdb {
namespace port {

// install a signal handler to print callstack on the following signals:
// sigill sigsegv sigbus sigabrt
// currently supports linux only. no-op otherwise.
void installstacktracehandler();

// prints stack, skips skip_first_frames frames
void printstack(int first_frames_to_skip = 0);

}  // namespace port
}  // namespace rocksdb
