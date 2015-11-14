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

#ifndef beast_config_contractchecks_h_included
#define beast_config_contractchecks_h_included

// this file has to work when included in a c source file.

#if defined (fatal_error) || \
    defined (fatal_condition) || \
    defined (meets_condition) || \
    defined (meets_precondition) || \
    defined (meets_postcondition) || \
    defined (meets_invariant) || \
    defined (check_invariant)
#error "programming by contract macros cannot be overriden!"
#endif

/** report a fatal error message and terminate the application.
    this macro automatically fills in the file and line number
    meets this declaration syntax:
    @code inline void fatal_error (char const* message); @endif
    @see fatalerror
*/
#define fatal_error(message) beast_reportfatalerror (message, __file__, __line__)

/** reports a fatal error message type if the condition is false
    the condition is always evaluated regardless of settings.
    meets this declaration syntax:
    @code inline void fatal_condition (bool condition, char const* category); @endcode
*/
#define fatal_condition(condition,category) static_cast <void> \
    (((!!(condition)) || (beast_reportfatalerror ( \
        category " '" beast_stringify(condition) "' failed.", __file__, __line__), 0)))

/** reports a fatal error message type if the condition is false
    the condition is always evaluated regardless of settings.
    meets this declaration syntax:
    @code inline void fatal_condition (bool condition, char const* category); @endcode
*/
#define meets_condition(condition,category) static_cast <bool> \
    (((!!(condition)) || (beast_reportfatalerror ( \
        category " '" beast_stringify(condition) "' failed.", __file__, __line__), false)))

/** condition tests for programming by contract.
    the condition is always evaluated regardless of settings, and gets returned.
    meets this declaration syntax:
    @code inline bool meets_condition (bool); @endcode
*/
/** @{ */
#define meets_precondition(condition)  meets_condition(condition,"pre-condition")
#define meets_postcondition(condition) meets_condition(condition,"post-condition")
#define meets_invariant(condition)     meets_condition(condition,"invariant")
/** @} */

/** condition tests for programming by contract.
    the condition is evaluated only if beast_disable_contract_checks is 0.
    meets this declaration syntax:
    @code inline void check_condition (bool); @endcode
    @see beast_disable_contract_checks
*/
/** @{ */
#if ! beast_disable_contract_checks
# define check_invariant(condition)     meets_invariant(condition)
#else
# define check_invariant(condition)     ((void)0)
#endif
/** @} */

#endif
