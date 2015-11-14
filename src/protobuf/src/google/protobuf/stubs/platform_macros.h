// protocol buffers - google's data interchange format
// copyright 2012 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

#ifndef google_protobuf_platform_macros_h_
#define google_protobuf_platform_macros_h_

#include <google/protobuf/stubs/common.h>

// processor architecture detection.  for more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -e -dm -"
#if defined(_m_x64) || defined(__x86_64__)
#define google_protobuf_arch_x64 1
#define google_protobuf_arch_64_bit 1
#elif defined(_m_ix86) || defined(__i386__)
#define google_protobuf_arch_ia32 1
#define google_protobuf_arch_32_bit 1
#elif defined(__qnx__)
#define google_protobuf_arch_arm_qnx 1
#define google_protobuf_arch_32_bit 1
#elif defined(__armel__)
#define google_protobuf_arch_arm 1
#define google_protobuf_arch_32_bit 1
#elif defined(__mipsel__)
#define google_protobuf_arch_mips 1
#define google_protobuf_arch_32_bit 1
#elif defined(__pnacl__)
#define google_protobuf_arch_32_bit 1
#elif defined(__ppc__)
#define google_protobuf_arch_ppc 1
#define google_protobuf_arch_32_bit 1
#else
#error host architecture was not detected as supported by protobuf
#endif

#if defined(__apple__)
#define google_protobuf_os_apple
#elif defined(__native_client__)
#define google_protobuf_os_nacl
#endif

#endif  // google_protobuf_platform_macros_h_
