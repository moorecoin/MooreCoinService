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

#ifndef ripple_basics_basicconfig_h_included
#define ripple_basics_basicconfig_h_included

#include <beast/container/const_container.h>
#include <beast/utility/ci_char_traits.h>
#include <boost/lexical_cast.hpp>
#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace ripple {

using inifilesections = std::map<std::string, std::vector<std::string>>;

//------------------------------------------------------------------------------

/** holds a collection of configuration values.
    a configuration file contains zero or more sections.
*/
class section
    : public beast::const_container <
        std::map <std::string, std::string, beast::ci_less>>
{
private:
    std::string name_;
    std::vector <std::string> lines_;
    std::vector <std::string> values_;

public:
    /** create an empty section. */
    explicit
    section (std::string const& name = "");

    /** returns the name of this section. */
    std::string const&
    name() const
    {
        return name_;
    }

    /** returns all the lines in the section.
        this includes everything.
    */
    std::vector <std::string> const&
    lines() const
    {
        return lines_;
    }

    /** returns all the values in the section.
        values are non-empty lines which are not key/value pairs.
    */
    std::vector <std::string> const&
    values() const
    {
        return values_;
    }

    /** set a key/value pair.
        the previous value is discarded.
    */
    void
    set (std::string const& key, std::string const& value);

    /** append a set of lines to this section.
        lines containing key/value pairs are added to the map,
        else they are added to the values list. everything is
        added to the lines list.
    */
    void
    append (std::vector <std::string> const& lines);

    /** append a line to this section. */
    void
    append (std::string const& line)
    {
        append (std::vector<std::string>{ line });
    }

    /** returns `true` if a key with the given name exists. */
    bool
    exists (std::string const& name) const;

    /** retrieve a key/value pair.
        @return a pair with bool `true` if the string was found.
    */
    std::pair <std::string, bool>
    find (std::string const& name) const;

    friend
    std::ostream&
    operator<< (std::ostream&, section const& section);
};

//------------------------------------------------------------------------------

/** holds unparsed configuration information.
    the raw data sections are processed with intermediate parsers specific
    to each module instead of being all parsed in a central location.
*/
class basicconfig
{
private:
    std::map <std::string, section, beast::ci_less> map_;

public:
    /** returns `true` if a section with the given name exists. */
    bool
    exists (std::string const& name) const;

    /** returns the section with the given name.
        if the section does not exist, an empty section is returned.
    */
    /** @{ */
    section const&
    section (std::string const& name) const;

    section const&
    operator[] (std::string const& name) const
    {
        return section(name);
    }
    /** @} */

    /** overwrite a key/value pair with a command line argument
        if the section does not exist it is created.
        the previous value, if any, is overwritten.
    */
    void
    overwrite (std::string const& section, std::string const& key,
        std::string const& value);

    friend
    std::ostream&
    operator<< (std::ostream& ss, basicconfig const& c);

protected:
    void
    build (inifilesections const& ifs);

    /** insert a legacy single section as a key/value pair.
        does nothing if the section does not exist, or does not contain
        a single line that is not a key/value pair.
        @deprecated
    */
    void
    remap (std::string const& legacy_section,
        std::string const& key, std::string const& new_section);
};

//------------------------------------------------------------------------------

/** set a value from a configuration section
    if the named value is not found, the variable is unchanged.
    @return `true` if value was set.
*/
template <class t>
bool
set (t& target, std::string const& name, section const& section)
{
    auto const result = section.find (name);
    if (! result.second)
        return false;
    try
    {
        target = boost::lexical_cast <t> (result.first);
        return true;
    }
    catch(...)
    {
    }
    return false;
}

/** set a value from a configuration section
    if the named value is not found, the variable is assigned the default.
    @return `true` if named value was found in the section.
*/
template <class t>
bool
set (t& target, t const& defaultvalue,
    std::string const& name, section const& section)
{
    auto const result = section.find (name);
    if (! result.second)
        return false;
    try
    {
        // vfalco todo use try_lexical_convert (boost 1.56.0)
        target = boost::lexical_cast <t> (result.first);
        return true;
    }
    catch(...)
    {
        target = defaultvalue;
    }
    return false;
}

/** retrieve a key/value pair from a section.
    @return the value string converted to t if it exists
            and can be parsed, or else defaultvalue.
*/
// note this routine might be more clumsy than the previous two
template <class t>
t
get (section const& section,
    std::string const& name, t const& defaultvalue = t{})
{
    auto const result = section.find (name);
    if (! result.second)
        return defaultvalue;
    try
    {
        return boost::lexical_cast <t> (result.first);
    }
    catch(...)
    {
    }
    return defaultvalue;
}

} // ripple

#endif
