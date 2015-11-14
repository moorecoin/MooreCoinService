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

enum
{
    u_isofs_super_magic = 5,
    u_msdos_super_magic = 2,
    u_nfs_super_magic = 1,
    u_smb_super_magic = 8
};

//==============================================================================
bool file::copyinternal (const file& dest) const
{
    fileinputstream in (*this);

    if (dest.deletefile())
    {
        {
            fileoutputstream out (dest);

            if (out.failedtoopen())
                return false;

            if (out.writefrominputstream (in, -1) == getsize())
                return true;
        }

        dest.deletefile();
    }

    return false;
}

//==============================================================================
static file resolvexdgfolder (const char* const type, const char* const fallbackfolder)
{
    file userdirs ("~/.config/user-dirs.dirs");
    stringarray conflines;

    if (userdirs.existsasfile())
    {
        fileinputstream in (userdirs);

        if (in.openedok())
            conflines.addlines (in.readentirestreamasstring());
    }

    for (int i = 0; i < conflines.size(); ++i)
    {
        const string line (conflines[i].trimstart());

        if (line.startswith (type))
        {
            // eg. resolve xdg_music_dir="$home/music" to /home/user/music
            const file f (line.replace ("$home", file ("~").getfullpathname())
                              .fromfirstoccurrenceof ("=", false, false)
                              .trim().unquoted());

            if (f.isdirectory())
                return f;
        }
    }

    return file (fallbackfolder);
}

const char* const* beast_argv = nullptr;
int beast_argc = 0;

file file::getspeciallocation (const speciallocationtype type)
{
    switch (type)
    {
        case userhomedirectory:
        {
            const char* homedir = getenv ("home");

            if (homedir == nullptr)
            {
                struct passwd* const pw = getpwuid (getuid());
                if (pw != nullptr)
                    homedir = pw->pw_dir;
            }

            return file (charpointer_utf8 (homedir));
        }

        case userdocumentsdirectory:          return resolvexdgfolder ("xdg_documents_dir", "~");
        case usermusicdirectory:              return resolvexdgfolder ("xdg_music_dir",     "~");
        case usermoviesdirectory:             return resolvexdgfolder ("xdg_videos_dir",    "~");
        case userpicturesdirectory:           return resolvexdgfolder ("xdg_pictures_dir",  "~");
        case userdesktopdirectory:            return resolvexdgfolder ("xdg_desktop_dir",   "~/desktop");
        case userapplicationdatadirectory:    return file ("~");
        case commonapplicationdatadirectory:  return file ("/var");
        case globalapplicationsdirectory:     return file ("/usr");

        case tempdirectory:
        {
            file tmp ("/var/tmp");

            if (! tmp.isdirectory())
            {
                tmp = "/tmp";

                if (! tmp.isdirectory())
                    tmp = file::getcurrentworkingdirectory();
            }

            return tmp;
        }

        default:
            bassertfalse; // unknown type?
            break;
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
          dir (opendir (directory.getfullpathname().toutf8()))
    {
    }

    pimpl (pimpl const&) = delete;
    pimpl& operator= (pimpl const&) = delete;

    ~pimpl()
    {
        if (dir != nullptr)
            closedir (dir);
    }

    bool next (string& filenamefound,
               bool* const isdir, bool* const ishidden, std::int64_t* const filesize,
               time* const modtime, time* const creationtime, bool* const isreadonly)
    {
        if (dir != nullptr)
        {
            const char* wildcardutf8 = nullptr;

            for (;;)
            {
                struct dirent* const de = readdir (dir);

                if (de == nullptr)
                    break;

                if (wildcardutf8 == nullptr)
                    wildcardutf8 = wildcard.toutf8();

                if (fnmatch (wildcardutf8, de->d_name, fnm_casefold) == 0)
                {
                    filenamefound = charpointer_utf8 (de->d_name);

                    updatestatinfoforfile (parentdir + filenamefound, isdir, filesize, modtime, creationtime, isreadonly);

                    if (ishidden != nullptr)
                        *ishidden = filenamefound.startswithchar ('.');

                    return true;
                }
            }
        }

        return false;
    }

private:
    string parentdir, wildcard;
    dir* dir;
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
