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

#include <beast/cxx14/memory.h> // <memory>

namespace beast {

static stringarray parsewildcards (const string& pattern)
{
    stringarray s;
    s.addtokens (pattern, ";,", "\"'");
    s.trim();
    s.removeemptystrings();
    return s;
}

static bool filematches (const stringarray& wildcards, const string& filename)
{
    for (int i = 0; i < wildcards.size(); ++i)
        if (filename.matcheswildcard (wildcards[i], ! file::arefilenamescasesensitive()))
            return true;

    return false;
}

directoryiterator::directoryiterator (const file& directory, bool recursive,
                                      const string& pattern, const int type)
  : wildcards (parsewildcards (pattern)),
    filefinder (directory, (recursive || wildcards.size() > 1) ? "*" : pattern),
    wildcard (pattern),
    path (file::addtrailingseparator (directory.getfullpathname())),
    index (-1),
    totalnumfiles (-1),
    whattolookfor (type),
    isrecursive (recursive),
    hasbeenadvanced (false)
{
    // you have to specify the type of files you're looking for!
    bassert ((type & (file::findfiles | file::finddirectories)) != 0);
    bassert (type > 0 && type <= 7);
}

directoryiterator::~directoryiterator()
{
}

bool directoryiterator::next()
{
    return next (nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
}

bool directoryiterator::next (bool* const isdirresult, bool* const ishiddenresult, std::int64_t* const filesize,
                              time* const modtime, time* const creationtime, bool* const isreadonly)
{
    hasbeenadvanced = true;

    if (subiterator != nullptr)
    {
        if (subiterator->next (isdirresult, ishiddenresult, filesize, modtime, creationtime, isreadonly))
            return true;

        subiterator = nullptr;
    }

    string filename;
    bool isdirectory, ishidden = false;

    while (filefinder.next (filename, &isdirectory,
                            (ishiddenresult != nullptr || (whattolookfor & file::ignorehiddenfiles) != 0) ? &ishidden : nullptr,
                            filesize, modtime, creationtime, isreadonly))
    {
        ++index;

        if (! filename.containsonly ("."))
        {
            bool matches = false;

            if (isdirectory)
            {
                if (isrecursive && ((whattolookfor & file::ignorehiddenfiles) == 0 || ! ishidden))
                    subiterator = std::make_unique <directoryiterator> (
                        file::createfilewithoutcheckingpath (path + filename),
                            true, wildcard, whattolookfor);

                matches = (whattolookfor & file::finddirectories) != 0;
            }
            else
            {
                matches = (whattolookfor & file::findfiles) != 0;
            }

            // if we're not relying on the os iterator to do the wildcard match, do it now..
            if (matches && (isrecursive || wildcards.size() > 1))
                matches = filematches (wildcards, filename);

            if (matches && (whattolookfor & file::ignorehiddenfiles) != 0)
                matches = ! ishidden;

            if (matches)
            {
                currentfile = file::createfilewithoutcheckingpath (path + filename);
                if (ishiddenresult != nullptr)     *ishiddenresult = ishidden;
                if (isdirresult != nullptr)        *isdirresult = isdirectory;

                return true;
            }

            if (subiterator != nullptr)
                return next (isdirresult, ishiddenresult, filesize, modtime, creationtime, isreadonly);
        }
    }

    return false;
}

const file& directoryiterator::getfile() const
{
    if (subiterator != nullptr && subiterator->hasbeenadvanced)
        return subiterator->getfile();

    // you need to call directoryiterator::next() before asking it for the file that it found!
    bassert (hasbeenadvanced);

    return currentfile;
}

float directoryiterator::getestimatedprogress() const
{
    if (totalnumfiles < 0)
        totalnumfiles = file (path).getnumberofchildfiles (file::findfilesanddirectories);

    if (totalnumfiles <= 0)
        return 0.0f;

    const float detailedindex = (subiterator != nullptr) ? index + subiterator->getestimatedprogress()
                                                         : (float) index;

    return detailedindex / totalnumfiles;
}

} // beast
