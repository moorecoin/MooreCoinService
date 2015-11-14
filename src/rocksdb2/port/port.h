//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_leveldb_port_port_h_
#define storage_leveldb_port_port_h_

#include <string.h>

// include the appropriate platform specific file below.  if you are
// porting to a new platform, see "port_example.h" for documentation
// of what the new port_<platform>.h file must provide.
#if defined(rocksdb_platform_posix)
#  include "port/port_posix.h"
#endif

#endif  // storage_leveldb_port_port_h_
