//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_utility_debug_h_included
#define beast_utility_debug_h_included

#include <beast/strings/string.h>
    
namespace beast {

// auxiliary outines for debugging

namespace debug
{

/** break to debugger if a debugger is attached to a debug build.

    does nothing if no debugger is attached, or the build is not a debug build.
*/
extern void breakpoint ();

/** given a file and line number this formats a suitable string.
    usually you will pass __file__ and __line__ here.
*/
string getsourcelocation (char const* filename, int linenumber,
                          int numberofparents = 0);

/** retrieve the file name from a full path.
    the nubmer of parents can be chosen
*/
string getfilenamefrompath (const char* sourcefilename, int numberofparents = 0);

//
// these control the msvc c runtime debug heap.
//
// the calls currently do nothing on other platforms.
//

/** calls checkheap() at every allocation and deallocation.
*/
extern void setalwayscheckheap (bool balwayscheck);

/** keep freed memory blocks in the heap's linked list, assign them the
    _free_block type, and fill them with the byte value 0xdd.
*/
extern void setheapdelayedfree (bool bdelayedfree);

/** perform automatic leak checking at program exit through a call to
    dumpmemoryleaks() and generate an error report if the application
    failed to free all the memory it allocated.
*/
extern void setheapreportleaks (bool breportleaks);

/** report all memory blocks which have not been freed.
*/
extern void reportleaks ();

/** confirms the integrity of the memory blocks allocated in the
    debug heap (debug version only.
*/
extern void checkheap ();

}

}

#endif
