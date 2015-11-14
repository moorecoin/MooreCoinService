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

#include <beastconfig.h>
#include <ripple/basics/basicconfig.h>
#include <boost/regex.hpp>
#include <algorithm>

namespace ripple {

section::section (std::string const& name)
    : name_(name)
{
}

void
section::set (std::string const& key, std::string const& value)
{
    auto const result = cont().emplace (key, value);
    if (! result.second)
        result.first->second = value;
}

void
section::append (std::vector <std::string> const& lines)
{
    // <key> '=' <value>
    static boost::regex const re1 (
        "^"                         // start of line
        "(?:\\s*)"                  // whitespace (optonal)
        "([a-za-z][_a-za-z0-9]*)"   // <key>
        "(?:\\s*)"                  // whitespace (optional)
        "(?:=)"                     // '='
        "(?:\\s*)"                  // whitespace (optional)
        "(.*\\s+)"                  // <value>
        "(?:\\s*)"                  // whitespace (optional)
        , boost::regex_constants::optimize
    );

    lines_.reserve (lines_.size() + lines.size());
    for (auto const& line : lines)
    {
        boost::smatch match;
        lines_.push_back (line);
        if (boost::regex_match (line, match, re1))
            set (match[1], match[2]);
        else
            values_.push_back (line);
    }
}

bool
section::exists (std::string const& name) const
{
    return cont().find (name) != cont().end();
}

std::pair <std::string, bool>
section::find (std::string const& name) const
{
    auto const iter = cont().find (name);
    if (iter == cont().end())
        return {{}, false};
    return {iter->second, true};
}

std::ostream&
operator<< (std::ostream& os, section const& section)
{
    for (auto const& kv : section.cont())
        os << kv.first << "=" << kv.second << "\n";
    return os;
}

//------------------------------------------------------------------------------

bool
basicconfig::exists (std::string const& name) const
{
    return map_.find(name) != map_.end();
}

section const&
basicconfig::section (std::string const& name) const
{
    static section none("");
    auto const iter = map_.find (name);
    if (iter == map_.end())
        return none;
    return iter->second;
}

void
basicconfig::remap (std::string const& legacy_section,
    std::string const& key, std::string const& new_section)
{
    auto const iter = map_.find (legacy_section);
    if (iter == map_.end())
        return;
    if (iter->second.size() != 0)
        return;
    if (iter->second.lines().size() != 1)
        return;
    auto result = map_.emplace(std::piecewise_construct,
        std::make_tuple(new_section), std::make_tuple(new_section));
    auto& s = result.first->second;
    s.append (iter->second.lines().front());
    s.set (key, iter->second.lines().front());
}

void
basicconfig::overwrite (std::string const& section, std::string const& key,
    std::string const& value)
{
    auto const result = map_.emplace (std::piecewise_construct,
        std::make_tuple(section), std::make_tuple(section));
    result.first->second.set (key, value);
}

void
basicconfig::build (inifilesections const& ifs)
{
    for (auto const& entry : ifs)
    {
        auto const result = map_.emplace (std::piecewise_construct,
            std::make_tuple(entry.first), std::make_tuple(entry.first));
        result.first->second.append (entry.second);
    }
}

std::ostream&
operator<< (std::ostream& ss, basicconfig const& c)
{
    for (auto const& s : c.map_)
        ss << "[" << s.first << "]\n" << s.second;
    return ss;
}

} // ripple
