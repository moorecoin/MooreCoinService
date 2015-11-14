//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#ifndef cpptl_json_features_h_included
#define cpptl_json_features_h_included

#include <ripple/json/json_config.h>

namespace json
{

/** \brief configuration passed to reader and writer.
 * this configuration object can be used to force the reader or writer
 * to behave in a standard conforming way.
 */
class json_api features
{
public:
    /** \brief a configuration that allows all features and assumes all strings are utf-8.
     * - c & c++ comments are allowed
     * - root object can be any json value
     * - assumes value strings are encoded in utf-8
     */
    static features all ();

    /** \brief a configuration that is strictly compatible with the json specification.
     * - comments are forbidden.
     * - root object must be either an array or an object value.
     * - assumes value strings are encoded in utf-8
     */
    static features strictmode ();

    /** \brief initialize the configuration like jsonconfig::allfeatures;
     */
    features ();

    /// \c true if comments are allowed. default: \c true.
    bool allowcomments_;

    /// \c true if root must be either an array or an object value. default: \c false.
    bool strictroot_;
};

} // namespace json

#endif // cpptl_json_features_h_included
