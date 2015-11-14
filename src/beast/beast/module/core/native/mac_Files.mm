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

namespace beast
{

/*
    note that a lot of methods that you'd expect to find in this file actually
    live in beast_posix_sharedcode.h!
*/

//==============================================================================
bool file::copyinternal (const file& dest) const
{
    beast_autoreleasepool
    {
        nsfilemanager* fm = [nsfilemanager defaultmanager];

        return [fm fileexistsatpath: beaststringtons (fullpath)]
               #if defined (mac_os_x_version_10_6) && mac_os_x_version_max_allowed >= mac_os_x_version_10_6
                && [fm copyitematpath: beaststringtons (fullpath)
                               topath: beaststringtons (dest.getfullpathname())
                                error: nil];
               #else
                && [fm copypath: beaststringtons (fullpath)
                         topath: beaststringtons (dest.getfullpathname())
                        handler: nil];
               #endif
    }
}

//==============================================================================
namespace filehelpers
{
    static bool isfileondrivetype (const file& f, const char* const* types)
    {
        struct statfs buf;

        if (beast_dostatfs (f, buf))
        {
            const string type (buf.f_fstypename);

            while (*types != 0)
                if (type.equalsignorecase (*types++))
                    return true;
        }

        return false;
    }

    static bool ishiddenfile (const string& path)
    {
       #if defined (mac_os_x_version_10_6) && mac_os_x_version_min_required >= mac_os_x_version_10_6
        beast_autoreleasepool
        {
            nsnumber* hidden = nil;
            nserror* err = nil;

            return [[nsurl fileurlwithpath: beaststringtons (path)]
                        getresourcevalue: &hidden forkey: nsurlishiddenkey error: &err]
                    && [hidden boolvalue];
        }
       #elif beast_ios
        return file (path).getfilename().startswithchar ('.');
       #else
        fsref ref;
        lsiteminforecord info;

        return fspathmakerefwithoptions ((const uint8*) path.torawutf8(), kfspathmakerefdonotfollowleafsymlink, &ref, 0) == noerr
                 && lscopyiteminfoforref (&ref, klsrequestbasicflagsonly, &info) == noerr
                 && (info.flags & klsiteminfoisinvisible) != 0;
       #endif
    }

   #if beast_ios
    string getiossystemlocation (nssearchpathdirectory type)
    {
        return nsstringtobeast ([nssearchpathfordirectoriesindomains (type, nsuserdomainmask, yes)
                                objectatindex: 0]);
    }
   #endif

    static bool launchexecutable (const string& pathandarguments)
    {
        const char* const argv[4] = { "/bin/sh", "-c", pathandarguments.toutf8(), 0 };

        const int cpid = fork();

        if (cpid == 0)
        {
            // child process
            if (execve (argv[0], (char**) argv, 0) < 0)
                exit (0);
        }
        else
        {
            if (cpid < 0)
                return false;
        }

        return true;
    }
}

//==============================================================================
const char* const* beast_argv = nullptr;
int beast_argc = 0;

file file::getspeciallocation (const speciallocationtype type)
{
    beast_autoreleasepool
    {
        string resultpath;

        switch (type)
        {
            case userhomedirectory:                 resultpath = nsstringtobeast (nshomedirectory()); break;

          #if beast_ios
            case userdocumentsdirectory:            resultpath = filehelpers::getiossystemlocation (nsdocumentdirectory); break;
            case userdesktopdirectory:              resultpath = filehelpers::getiossystemlocation (nsdesktopdirectory); break;

            case tempdirectory:
            {
                file tmp (filehelpers::getiossystemlocation (nscachesdirectory));
                tmp = tmp.getchildfile (beast_getexecutablefile().getfilenamewithoutextension());
                tmp.createdirectory();
                return tmp.getfullpathname();
            }

          #else
            case userdocumentsdirectory:            resultpath = "~/documents"; break;
            case userdesktopdirectory:              resultpath = "~/desktop"; break;

            case tempdirectory:
            {
                file tmp ("~/library/caches/" + beast_getexecutablefile().getfilenamewithoutextension());
                tmp.createdirectory();
                return tmp.getfullpathname();
            }
          #endif
            case usermusicdirectory:                resultpath = "~/music"; break;
            case usermoviesdirectory:               resultpath = "~/movies"; break;
            case userpicturesdirectory:             resultpath = "~/pictures"; break;
            case userapplicationdatadirectory:      resultpath = "~/library"; break;
            case commonapplicationdatadirectory:    resultpath = "/library"; break;
            case commondocumentsdirectory:          resultpath = "/users/shared"; break;
            case globalapplicationsdirectory:       resultpath = "/applications"; break;

            default:
                bassertfalse; // unknown type?
                break;
        }

        if (resultpath.isnotempty())
            return file (resultpath.converttoprecomposedunicode());
    }

    return file::nonexistent ();
}

//==============================================================================
class directoryiterator::nativeiterator::pimpl
{
public:
    pimpl (const file& directory, const string& wildcard_)
        : parentdir (file::addtrailingseparator (directory.getfullpathname())),
          wildcard (wildcard_),
          enumerator (nil)
    {
        beast_autoreleasepool
        {
            enumerator = [[[nsfilemanager defaultmanager] enumeratoratpath: beaststringtons (directory.getfullpathname())] retain];
        }
    }

    pimpl (pimpl const&) = delete;
    pimpl& operator= (pimpl const&) = delete;

    ~pimpl()
    {
        [enumerator release];
    }

    bool next (string& filenamefound,
               bool* const isdir, bool* const ishidden, std::int64_t* const filesize,
               time* const modtime, time* const creationtime, bool* const isreadonly)
    {
        beast_autoreleasepool
        {
            const char* wildcardutf8 = nullptr;

            for (;;)
            {
                nsstring* file;
                if (enumerator == nil || (file = [enumerator nextobject]) == nil)
                    return false;

                [enumerator skipdescendents];
                filenamefound = nsstringtobeast (file);

                if (wildcardutf8 == nullptr)
                    wildcardutf8 = wildcard.toutf8();

                if (fnmatch (wildcardutf8, filenamefound.toutf8(), fnm_casefold) != 0)
                    continue;

                const string fullpath (parentdir + filenamefound);
                updatestatinfoforfile (fullpath, isdir, filesize, modtime, creationtime, isreadonly);

                if (ishidden != nullptr)
                    *ishidden = filehelpers::ishiddenfile (fullpath);

                return true;
            }
        }
    }

private:
    string parentdir, wildcard;
    nsdirectoryenumerator* enumerator;
};

directoryiterator::nativeiterator::nativeiterator (const file& directory, const string& wildcard)
    : pimpl (new directoryiterator::nativeiterator::pimpl (directory, wildcard))
{
}

directoryiterator::nativeiterator::~nativeiterator()
{
}

bool directoryiterator::nativeiterator::next (string& filenamefound,
                                              bool* const isdir, bool* const ishidden, std::int64_t* const filesize,
                                              time* const modtime, time* const creationtime, bool* const isreadonly)
{
    return pimpl->next (filenamefound, isdir, ishidden, filesize, modtime, creationtime, isreadonly);
}

} // beast
