//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

    permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    the  software is provided "as is" and the author disclaims all warranties
    with  regard  to  this  software  including  all  implied  warranties  of
    merchantability  and  fitness. in no event shall the author be liable for
    any  special ,  direct, indirect, or consequential damages or any damages
    whatsoever  resulting  from  loss  of use, data or profits, whether in an
    action  of  contract, negligence or other tortious action, arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

#ifndef beast_core_h_included
#define beast_core_h_included

// targetplatform.h should not use anything from beastconfig.h
#include <beast/config.h>
#include <beast/config/contractchecks.h>

#if beast_msvc
# pragma warning (disable: 4251) // (dll build warning, must be disabled before pushing the warning state)
# pragma warning (push)
# pragma warning (disable: 4786) // (long class name warning)
# ifdef __intel_compiler
#  pragma warning (disable: 1125)
# endif
#endif

//------------------------------------------------------------------------------

// new header-only library modeled more closely according to boost
#include <beast/smartptr.h>
#include <beast/arithmetic.h>
#include <beast/byteorder.h>
#include <beast/heapblock.h>
#include <beast/memory.h>
#include <beast/intrusive.h>
#include <beast/strings.h>
#include <beast/threads.h>

#include <beast/utility/debug.h>
#include <beast/utility/error.h>
#include <beast/utility/journal.h>
#include <beast/utility/leakchecked.h>
#include <beast/utility/propertystream.h>
#include <beast/utility/staticobject.h>

#include <beast/module/core/system/standardincludes.h>

namespace beast
{

class inputstream;
class outputstream;
class fileinputstream;
class fileoutputstream;

} // beast

// order matters, since headers don't have their own #include lines.
// add new includes to the bottom.

#include <beast/module/core/time/atexithook.h>
#include <beast/module/core/time/time.h>
#include <beast/module/core/threads/scopedlock.h>
#include <beast/module/core/threads/criticalsection.h>
#include <beast/module/core/containers/elementcomparator.h>

// if the msvc debug heap headers were included, disable
// the macros during the juce include since they conflict.
#ifdef _crtdbg_map_alloc
#pragma push_macro("calloc")
#pragma push_macro("free")
#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("_recalloc")
#pragma push_macro("_aligned_free")
#pragma push_macro("_aligned_malloc")
#pragma push_macro("_aligned_offset_malloc")
#pragma push_macro("_aligned_realloc")
#pragma push_macro("_aligned_recalloc")
#pragma push_macro("_aligned_offset_realloc")
#pragma push_macro("_aligned_offset_recalloc")
#pragma push_macro("_aligned_msize")
#undef calloc
#undef free
#undef malloc
#undef realloc
#undef _recalloc
#undef _aligned_free
#undef _aligned_malloc
#undef _aligned_offset_malloc
#undef _aligned_realloc
#undef _aligned_recalloc
#undef _aligned_offset_realloc
#undef _aligned_offset_recalloc
#undef _aligned_msize
#endif
#include <beast/module/core/containers/arrayallocationbase.h>
#ifdef _crtdbg_map_alloc
#pragma pop_macro("_aligned_msize")
#pragma pop_macro("_aligned_offset_recalloc")
#pragma pop_macro("_aligned_offset_realloc")
#pragma pop_macro("_aligned_recalloc")
#pragma pop_macro("_aligned_realloc")
#pragma pop_macro("_aligned_offset_malloc")
#pragma pop_macro("_aligned_malloc")
#pragma pop_macro("_aligned_free")
#pragma pop_macro("_recalloc")
#pragma pop_macro("realloc")
#pragma pop_macro("malloc")
#pragma pop_macro("free")
#pragma pop_macro("calloc")
#endif

#include <beast/module/core/containers/array.h>

#include <beast/module/core/misc/result.h>
#include <beast/module/core/text/stringarray.h>
#include <beast/module/core/memory/memoryblock.h>
#include <beast/module/core/files/file.h>

#include <beast/module/core/thread/mutextraits.h>
#include <beast/module/core/diagnostic/fatalerror.h>
#include <beast/module/core/text/lexicalcast.h>
#include <beast/module/core/logging/logger.h>
#include <beast/module/core/maths/random.h>
#include <beast/module/core/text/stringpairarray.h>
#include <beast/module/core/files/directoryiterator.h>
#include <beast/module/core/streams/inputstream.h>
#include <beast/module/core/files/fileinputstream.h>
#include <beast/module/core/streams/inputsource.h>
#include <beast/module/core/streams/fileinputsource.h>
#include <beast/module/core/streams/outputstream.h>
#include <beast/module/core/files/fileoutputstream.h>
#include <beast/module/core/memory/sharedsingleton.h>
#include <beast/module/core/streams/memoryoutputstream.h>

#include <beast/module/core/system/systemstats.h>
#include <beast/module/core/diagnostic/semanticversion.h>
#include <beast/module/core/threads/process.h>
#include <beast/module/core/diagnostic/unittestutilities.h>

#include <beast/module/core/diagnostic/measurefunctioncalltime.h>

#include <beast/module/core/thread/deadlinetimer.h>

#include <beast/module/core/thread/workers.h>

#if beast_msvc
#pragma warning (pop)
#endif

#endif
