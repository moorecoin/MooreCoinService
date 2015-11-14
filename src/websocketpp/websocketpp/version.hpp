/*
 * copyright (c) 2014, peter thorson. all rights reserved.
 *
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * neither the name of the websocket++ project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * this software is provided by the copyright holders and contributors "as is"
 * and any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed. in no event shall peter thorson be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused and
 * on any theory of liability, whether in contract, strict liability, or tort
 * (including negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 *
 */

#ifndef websocketpp_version_hpp
#define websocketpp_version_hpp

/// namespace for the websocket++ project
namespace websocketpp {

/*
 other places where version information is kept
 - readme.md
 - changelog.md
 - doxyfile
 - cmakelists.txt
*/

/// library major version number
static int const major_version = 0;
/// library minor version number
static int const minor_version = 4;
/// library patch version number
static int const patch_version = 0;
/// library pre-release flag
/**
 * this is a textual flag indicating the type and number for pre-release
 * versions (dev, alpha, beta, rc). this will be blank for release versions.
 */
static char const prerelease_flag[] = "";

/// default user agent string
static char const user_agent[] = "websocket++/0.4.0";

} // namespace websocketpp

#endif // websocketpp_version_hpp
