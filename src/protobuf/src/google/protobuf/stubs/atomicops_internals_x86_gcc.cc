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

// this module gets enough cpu information to optimize the
// atomicops module on x86.

#include <cstring>

#include <google/protobuf/stubs/atomicops.h>

// this file only makes sense with atomicops_internals_x86_gcc.h -- it
// depends on structs that are defined in that file.  if atomicops.h
// doesn't sub-include that file, then we aren't needed, and shouldn't
// try to do anything.
#ifdef google_protobuf_atomicops_internals_x86_gcc_h_

// inline cpuid instruction.  in pic compilations, %ebx contains the address
// of the global offset table.  to avoid breaking such executables, this code
// must preserve that register's value across cpuid instructions.
#if defined(__i386__)
#define cpuid(a, b, c, d, inp) \
  asm("mov %%ebx, %%edi\n"     \
      "cpuid\n"                \
      "xchg %%edi, %%ebx\n"    \
      : "=a" (a), "=d" (b), "=c" (c), "=d" (d) : "a" (inp))
#elif defined(__x86_64__)
#define cpuid(a, b, c, d, inp) \
  asm("mov %%rbx, %%rdi\n"     \
      "cpuid\n"                \
      "xchg %%rdi, %%rbx\n"    \
      : "=a" (a), "=d" (b), "=c" (c), "=d" (d) : "a" (inp))
#endif

#if defined(cpuid)        // initialize the struct only on x86

namespace google {
namespace protobuf {
namespace internal {

// set the flags so that code will run correctly and conservatively, so even
// if we haven't been initialized yet, we're probably single threaded, and our
// default values should hopefully be pretty safe.
struct atomicops_x86cpufeaturestruct atomicops_internalx86cpufeatures = {
  false,          // bug can't exist before process spawns multiple threads
  false,          // no sse2
};

namespace {

// initialize the atomicops_internalx86cpufeatures struct.
void atomicops_internalx86cpufeaturesinit() {
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;

  // get vendor string (issue cpuid with eax = 0)
  cpuid(eax, ebx, ecx, edx, 0);
  char vendor[13];
  memcpy(vendor, &ebx, 4);
  memcpy(vendor + 4, &edx, 4);
  memcpy(vendor + 8, &ecx, 4);
  vendor[12] = 0;

  // get feature flags in ecx/edx, and family/model in eax
  cpuid(eax, ebx, ecx, edx, 1);

  int family = (eax >> 8) & 0xf;        // family and model fields
  int model = (eax >> 4) & 0xf;
  if (family == 0xf) {                  // use extended family and model fields
    family += (eax >> 20) & 0xff;
    model += ((eax >> 16) & 0xf) << 4;
  }

  // opteron rev e has a bug in which on very rare occasions a locked
  // instruction doesn't act as a read-acquire barrier if followed by a
  // non-locked read-modify-write instruction.  rev f has this bug in
  // pre-release versions, but not in versions released to customers,
  // so we test only for rev e, which is family 15, model 32..63 inclusive.
  if (strcmp(vendor, "authenticamd") == 0 &&       // amd
      family == 15 &&
      32 <= model && model <= 63) {
    atomicops_internalx86cpufeatures.has_amd_lock_mb_bug = true;
  } else {
    atomicops_internalx86cpufeatures.has_amd_lock_mb_bug = false;
  }

  // edx bit 26 is sse2 which we use to tell use whether we can use mfence
  atomicops_internalx86cpufeatures.has_sse2 = ((edx >> 26) & 1);
}

class atomicopsx86initializer {
 public:
  atomicopsx86initializer() {
    atomicops_internalx86cpufeaturesinit();
  }
};

// a global to get use initialized on startup via static initialization :/
atomicopsx86initializer g_initer;

}  // namespace

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // __i386__

#endif  // google_protobuf_atomicops_internals_x86_gcc_h_
