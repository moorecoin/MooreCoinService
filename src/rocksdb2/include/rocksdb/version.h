// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
#pragma once

// also update makefile if you change these
#define rocksdb_major 3
#define rocksdb_minor 5
#define rocksdb_patch 1

// do not use these. we made the mistake of declaring macros starting with
// double underscore. now we have to live with our choice. we'll deprecate these
// at some point
#define __rocksdb_major__ rocksdb_major
#define __rocksdb_minor__ rocksdb_minor
#define __rocksdb_patch__ rocksdb_patch
