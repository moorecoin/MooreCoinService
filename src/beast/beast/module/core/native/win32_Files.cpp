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

#ifndef invalid_file_attributes
 #define invalid_file_attributes ((dword) -1)
#endif

//==============================================================================
namespace windowsfilehelpers
{
    static_assert (sizeof (ularge_integer) == sizeof (filetime),
        "the filetime structure format has been modified.");

    dword getatts (const string& path)
    {
        return getfileattributes (path.towidecharpointer());
    }

    std::int64_t filetimetotime (const filetime* const ft)
    {
        return (std::int64_t) ((reinterpret_cast<const ularge_integer*> (ft)->quadpart - 116444736000000000ll) / 10000);
    }

    filetime* timetofiletime (const std::int64_t time, filetime* const ft) noexcept
    {
        if (time <= 0)
            return nullptr;

        reinterpret_cast<ularge_integer*> (ft)->quadpart = (ulonglong) (time * 10000 + 116444736000000000ll);
        return ft;
    }

    string getdrivefrompath (string path)
    {
        if (path.isnotempty() && path[1] == ':' && path[2] == 0)
            path << '\\';

        const size_t numbytes = charpointer_utf16::getbytesrequiredfor (path.getcharpointer()) + 4;
        heapblock<wchar> pathcopy;
        pathcopy.calloc (numbytes, 1);
        path.copytoutf16 (pathcopy, numbytes);

        if (pathstriptoroot (pathcopy))
            path = static_cast <const wchar*> (pathcopy);

        return path;
    }

    std::int64_t getdiskspaceinfo (const string& path, const bool total)
    {
        ularge_integer spc, tot, totfree;

        if (getdiskfreespaceex (getdrivefrompath (path).towidecharpointer(), &spc, &tot, &totfree))
            return total ? (std::int64_t) tot.quadpart
                         : (std::int64_t) spc.quadpart;

        return 0;
    }

    unsigned int getwindowsdrivetype (const string& path)
    {
        return getdrivetype (getdrivefrompath (path).towidecharpointer());
    }

    file getspecialfolderpath (int type)
    {
        wchar path [max_path + 256];

        if (shgetspecialfolderpath (0, path, type, false))
            return file (string (path));

        return file::nonexistent ();
    }

    file getmodulefilename (hinstance modulehandle)
    {
        wchar dest [max_path + 256];
        dest[0] = 0;
        getmodulefilename (modulehandle, dest, (dword) numelementsinarray (dest));
        return file (string (dest));
    }

    result getresultforlasterror()
    {
        tchar messagebuffer [256] = { 0 };

        formatmessage (format_message_from_system | format_message_ignore_inserts,
                       nullptr, getlasterror(), makelangid (lang_neutral, sublang_default),
                       messagebuffer, (dword) numelementsinarray (messagebuffer) - 1, nullptr);

        return result::fail (string (messagebuffer));
    }
}

//==============================================================================
const beast_wchar file::separator = '\\';
const string file::separatorstring ("\\");


//==============================================================================
bool file::exists() const
{
    return fullpath.isnotempty()
            && windowsfilehelpers::getatts (fullpath) != invalid_file_attributes;
}

bool file::existsasfile() const
{
    return fullpath.isnotempty()
            && (windowsfilehelpers::getatts (fullpath) & file_attribute_directory) == 0;
}

bool file::isdirectory() const
{
    const dword attr = windowsfilehelpers::getatts (fullpath);
    return ((attr & file_attribute_directory) != 0) && (attr != invalid_file_attributes);
}

bool file::haswriteaccess() const
{
    if (exists())
        return (windowsfilehelpers::getatts (fullpath) & file_attribute_readonly) == 0;

    // on windows, it seems that even read-only directories can still be written into,
    // so checking the parent directory's permissions would return the wrong result..
    return true;
}

bool file::setfilereadonlyinternal (const bool shouldbereadonly) const
{
    const dword oldatts = windowsfilehelpers::getatts (fullpath);

    if (oldatts == invalid_file_attributes)
        return false;

    const dword newatts = shouldbereadonly ? (oldatts |  file_attribute_readonly)
                                           : (oldatts & ~file_attribute_readonly);
    return newatts == oldatts
            || setfileattributes (fullpath.towidecharpointer(), newatts) != false;
}

//==============================================================================
bool file::deletefile() const
{
    if (! exists())
        return true;

    return isdirectory() ? removedirectory (fullpath.towidecharpointer()) != 0
                         : deletefile (fullpath.towidecharpointer()) != 0;
}

bool file::copyinternal (const file& dest) const
{
    return copyfile (fullpath.towidecharpointer(), dest.getfullpathname().towidecharpointer(), false) != 0;
}

bool file::moveinternal (const file& dest) const
{
    return movefile (fullpath.towidecharpointer(), dest.getfullpathname().towidecharpointer()) != 0;
}

result file::createdirectoryinternal (const string& filename) const
{
    return createdirectory (filename.towidecharpointer(), 0) ? result::ok()
                                                             : windowsfilehelpers::getresultforlasterror();
}

//==============================================================================
std::int64_t beast_filesetposition (void* handle, std::int64_t pos)
{
    large_integer li;
    li.quadpart = pos;
    li.lowpart = setfilepointer ((handle) handle, (long) li.lowpart, &li.highpart, file_begin);  // (returns -1 if it fails)
    return li.quadpart;
}

void fileinputstream::openhandle()
{
    handle h = createfile (file.getfullpathname().towidecharpointer(), generic_read, file_share_read | file_share_write, 0,
                           open_existing, file_attribute_normal | file_flag_sequential_scan, 0);

    if (h != invalid_handle_value)
        filehandle = (void*) h;
    else
        status = windowsfilehelpers::getresultforlasterror();
}

void fileinputstream::closehandle()
{
    closehandle ((handle) filehandle);
}

size_t fileinputstream::readinternal (void* buffer, size_t numbytes)
{
    if (filehandle != 0)
    {
        dword actualnum = 0;
        if (! readfile ((handle) filehandle, buffer, (dword) numbytes, &actualnum, 0))
            status = windowsfilehelpers::getresultforlasterror();

        return (size_t) actualnum;
    }

    return 0;
}

//==============================================================================
void fileoutputstream::openhandle()
{
    handle h = createfile (file.getfullpathname().towidecharpointer(), generic_write, file_share_read, 0,
                           open_always, file_attribute_normal, 0);

    if (h != invalid_handle_value)
    {
        large_integer li;
        li.quadpart = 0;
        li.lowpart = setfilepointer (h, 0, &li.highpart, file_end);

        if (li.lowpart != invalid_set_file_pointer)
        {
            filehandle = (void*) h;
            currentposition = li.quadpart;
            return;
        }
    }

    status = windowsfilehelpers::getresultforlasterror();
}

void fileoutputstream::closehandle()
{
    closehandle ((handle) filehandle);
}

std::ptrdiff_t fileoutputstream::writeinternal (const void* buffer, size_t numbytes)
{
    if (filehandle != nullptr)
    {
        dword actualnum = 0;
        if (! writefile ((handle) filehandle, buffer, (dword) numbytes, &actualnum, 0))
            status = windowsfilehelpers::getresultforlasterror();

        return (std::ptrdiff_t) actualnum;
    }

    return 0;
}

void fileoutputstream::flushinternal()
{
    if (filehandle != nullptr)
        if (! flushfilebuffers ((handle) filehandle))
            status = windowsfilehelpers::getresultforlasterror();
}

result fileoutputstream::truncate()
{
    if (filehandle == nullptr)
        return status;

    flush();
    return setendoffile ((handle) filehandle) ? result::ok()
                                              : windowsfilehelpers::getresultforlasterror();
}

//==============================================================================

std::int64_t file::getsize() const
{
    win32_file_attribute_data attributes;

    if (getfileattributesex (fullpath.towidecharpointer(), getfileexinfostandard, &attributes))
        return (((std::int64_t) attributes.nfilesizehigh) << 32) | attributes.nfilesizelow;

    return 0;
}

void file::getfiletimesinternal (std::int64_t& modificationtime, std::int64_t& accesstime, std::int64_t& creationtime) const
{
    using namespace windowsfilehelpers;
    win32_file_attribute_data attributes;

    if (getfileattributesex (fullpath.towidecharpointer(), getfileexinfostandard, &attributes))
    {
        modificationtime = filetimetotime (&attributes.ftlastwritetime);
        creationtime     = filetimetotime (&attributes.ftcreationtime);
        accesstime       = filetimetotime (&attributes.ftlastaccesstime);
    }
    else
    {
        creationtime = accesstime = modificationtime = 0;
    }
}

bool file::setfiletimesinternal (std::int64_t modificationtime, std::int64_t accesstime, std::int64_t creationtime) const
{
    using namespace windowsfilehelpers;

    bool ok = false;
    handle h = createfile (fullpath.towidecharpointer(), generic_write, file_share_read, 0,
                           open_always, file_attribute_normal, 0);

    if (h != invalid_handle_value)
    {
        filetime m, a, c;

        ok = setfiletime (h,
                          timetofiletime (creationtime, &c),
                          timetofiletime (accesstime, &a),
                          timetofiletime (modificationtime, &m)) != 0;

        closehandle (h);
    }

    return ok;
}

//==============================================================================
std::int64_t file::getbytesfreeonvolume() const
{
    return windowsfilehelpers::getdiskspaceinfo (getfullpathname(), false);
}

std::int64_t file::getvolumetotalsize() const
{
    return windowsfilehelpers::getdiskspaceinfo (getfullpathname(), true);
}

//==============================================================================
file file::getspeciallocation (const speciallocationtype type)
{
    int csidltype = 0;

    switch (type)
    {
        case userhomedirectory:                 csidltype = csidl_profile; break;
        case userdocumentsdirectory:            csidltype = csidl_personal; break;
        case userdesktopdirectory:              csidltype = csidl_desktop; break;
        case userapplicationdatadirectory:      csidltype = csidl_appdata; break;
        case commonapplicationdatadirectory:    csidltype = csidl_common_appdata; break;
        case commondocumentsdirectory:          csidltype = csidl_common_documents; break;
        case globalapplicationsdirectory:       csidltype = csidl_program_files; break;
        case usermusicdirectory:                csidltype = 0x0d; /*csidl_mymusic*/ break;
        case usermoviesdirectory:               csidltype = 0x0e; /*csidl_myvideo*/ break;
        case userpicturesdirectory:             csidltype = 0x27; /*csidl_mypictures*/ break;

        case tempdirectory:
        {
            wchar dest [2048];
            dest[0] = 0;
            gettemppath ((dword) numelementsinarray (dest), dest);
            return file (string (dest));
        }

        default:
            bassertfalse; // unknown type?
            return file::nonexistent ();
    }

    return windowsfilehelpers::getspecialfolderpath (csidltype);
}

//==============================================================================
file file::getcurrentworkingdirectory()
{
    wchar dest [max_path + 256];
    dest[0] = 0;
    getcurrentdirectory ((dword) numelementsinarray (dest), dest);
    return file (string (dest));
}

bool file::setascurrentworkingdirectory() const
{
    return setcurrentdirectory (getfullpathname().towidecharpointer()) != false;
}

//==============================================================================
class directoryiterator::nativeiterator::pimpl
    : leakchecked <directoryiterator::nativeiterator::pimpl>
{
public:
    pimpl (const file& directory, const string& wildcard)
        : directorywithwildcard (file::addtrailingseparator (directory.getfullpathname()) + wildcard),
          handle (invalid_handle_value)
    {
    }

    pimpl (pimpl const&) = delete;
    pimpl& operator= (pimpl const&) = delete;

    ~pimpl()
    {
        if (handle != invalid_handle_value)
            findclose (handle);
    }

    bool next (string& filenamefound,
               bool* const isdir, bool* const ishidden, std::int64_t* const filesize,
               time* const modtime, time* const creationtime, bool* const isreadonly)
    {
        using namespace windowsfilehelpers;
        win32_find_data finddata;

        if (handle == invalid_handle_value)
        {
            handle = findfirstfile (directorywithwildcard.towidecharpointer(), &finddata);

            if (handle == invalid_handle_value)
                return false;
        }
        else
        {
            if (findnextfile (handle, &finddata) == 0)
                return false;
        }

        filenamefound = finddata.cfilename;

        if (isdir != nullptr)         *isdir        = ((finddata.dwfileattributes & file_attribute_directory) != 0);
        if (ishidden != nullptr)      *ishidden     = ((finddata.dwfileattributes & file_attribute_hidden) != 0);
        if (isreadonly != nullptr)    *isreadonly   = ((finddata.dwfileattributes & file_attribute_readonly) != 0);
        if (filesize != nullptr)      *filesize     = finddata.nfilesizelow + (((std::int64_t) finddata.nfilesizehigh) << 32);
        if (modtime != nullptr)       *modtime      = time (filetimetotime (&finddata.ftlastwritetime));
        if (creationtime != nullptr)  *creationtime = time (filetimetotime (&finddata.ftcreationtime));

        return true;
    }

private:
    const string directorywithwildcard;
    handle handle;
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
