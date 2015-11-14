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

criticalsection::criticalsection() noexcept
{
    pthread_mutexattr_t atts;
    pthread_mutexattr_init (&atts);
    pthread_mutexattr_settype (&atts, pthread_mutex_recursive);
   #if ! beast_android
    pthread_mutexattr_setprotocol (&atts, pthread_prio_inherit);
   #endif
    pthread_mutex_init (&mutex, &atts);
    pthread_mutexattr_destroy (&atts);
}

criticalsection::~criticalsection() noexcept    { pthread_mutex_destroy (&mutex); }
void criticalsection::enter() const noexcept    { pthread_mutex_lock (&mutex); }
bool criticalsection::tryenter() const noexcept { return pthread_mutex_trylock (&mutex) == 0; }
void criticalsection::exit() const noexcept     { pthread_mutex_unlock (&mutex); }

//==============================================================================

void process::terminate()
{
#if beast_android || beast_bsd
   // http://www.unix.com/man-page/freebsd/2/_exit/
    ::_exit (exit_failure);
#else
    std::_exit (exit_failure);
#endif
}

//==============================================================================
const beast_wchar file::separator = '/';
const string file::separatorstring ("/");

//==============================================================================
file file::getcurrentworkingdirectory()
{
    heapblock<char> heapbuffer;

    char localbuffer [1024];
    char* cwd = getcwd (localbuffer, sizeof (localbuffer) - 1);
    size_t buffersize = 4096;

    while (cwd == nullptr && errno == erange)
    {
        heapbuffer.malloc (buffersize);
        cwd = getcwd (heapbuffer, buffersize - 1);
        buffersize += 1024;
    }

    return file (charpointer_utf8 (cwd));
}

bool file::setascurrentworkingdirectory() const
{
    return chdir (getfullpathname().toutf8()) == 0;
}

//==============================================================================
// the unix siginterrupt function is deprecated - this does the same job.
int beast_siginterrupt (int sig, int flag)
{
    struct ::sigaction act;
    (void) ::sigaction (sig, nullptr, &act);

    if (flag != 0)
        act.sa_flags &= ~sa_restart;
    else
        act.sa_flags |= sa_restart;

    return ::sigaction (sig, &act, nullptr);
}

//==============================================================================
namespace
{
   #if beast_linux || \
       (beast_ios && ! __darwin_only_64_bit_ino_t) // (this ios stuff is to avoid a simulator bug)
    typedef struct stat64 beast_statstruct;
    #define beast_stat     stat64
   #else
    typedef struct stat   beast_statstruct;
    #define beast_stat     stat
   #endif

    bool beast_stat (const string& filename, beast_statstruct& info)
    {
        return filename.isnotempty()
                 && beast_stat (filename.toutf8(), &info) == 0;
    }

    // if this file doesn't exist, find a parent of it that does..
    bool beast_dostatfs (file f, struct statfs& result)
    {
        for (int i = 5; --i >= 0;)
        {
            if (f.exists())
                break;

            f = f.getparentdirectory();
        }

        return statfs (f.getfullpathname().toutf8(), &result) == 0;
    }

    void updatestatinfoforfile (const string& path, bool* const isdir, std::int64_t* const filesize,
                                time* const modtime, time* const creationtime, bool* const isreadonly)
    {
        if (isdir != nullptr || filesize != nullptr || modtime != nullptr || creationtime != nullptr)
        {
            beast_statstruct info;
            const bool statok = beast_stat (path, info);

            if (isdir != nullptr)         *isdir        = statok && ((info.st_mode & s_ifdir) != 0);
            if (filesize != nullptr)      *filesize     = statok ? info.st_size : 0;
            if (modtime != nullptr)       *modtime      = time (statok ? (std::int64_t) info.st_mtime * 1000 : 0);
            if (creationtime != nullptr)  *creationtime = time (statok ? (std::int64_t) info.st_ctime * 1000 : 0);
        }

        if (isreadonly != nullptr)
            *isreadonly = access (path.toutf8(), w_ok) != 0;
    }

    result getresultforerrno()
    {
        return result::fail (string (strerror (errno)));
    }

    result getresultforreturnvalue (int value)
    {
        return value == -1 ? getresultforerrno() : result::ok();
    }

    int getfd (void* handle) noexcept        { return (int) (std::intptr_t) handle; }
    void* fdtovoidpointer (int fd) noexcept  { return (void*) (std::intptr_t) fd; }
}

bool file::isdirectory() const
{
    beast_statstruct info;

    return fullpath.isempty()
            || (beast_stat (fullpath, info) && ((info.st_mode & s_ifdir) != 0));
}

bool file::exists() const
{
    return fullpath.isnotempty()
             && access (fullpath.toutf8(), f_ok) == 0;
}

bool file::existsasfile() const
{
    return exists() && ! isdirectory();
}

std::int64_t file::getsize() const
{
    beast_statstruct info;
    return beast_stat (fullpath, info) ? info.st_size : 0;
}

//==============================================================================
bool file::haswriteaccess() const
{
    if (exists())
        return access (fullpath.toutf8(), w_ok) == 0;

    if ((! isdirectory()) && fullpath.containschar (separator))
        return getparentdirectory().haswriteaccess();

    return false;
}

bool file::setfilereadonlyinternal (const bool shouldbereadonly) const
{
    beast_statstruct info;
    if (! beast_stat (fullpath, info))
        return false;

    info.st_mode &= 0777;   // just permissions

    if (shouldbereadonly)
        info.st_mode &= ~(s_iwusr | s_iwgrp | s_iwoth);
    else
        // give everybody write permission?
        info.st_mode |= s_iwusr | s_iwgrp | s_iwoth;

    return chmod (fullpath.toutf8(), info.st_mode) == 0;
}

void file::getfiletimesinternal (std::int64_t& modificationtime, std::int64_t& accesstime, std::int64_t& creationtime) const
{
    modificationtime = 0;
    accesstime = 0;
    creationtime = 0;

    beast_statstruct info;
    if (beast_stat (fullpath, info))
    {
        modificationtime = (std::int64_t) info.st_mtime * 1000;
        accesstime = (std::int64_t) info.st_atime * 1000;
        creationtime = (std::int64_t) info.st_ctime * 1000;
    }
}

bool file::setfiletimesinternal (std::int64_t modificationtime, std::int64_t accesstime, std::int64_t /*creationtime*/) const
{
    beast_statstruct info;

    if ((modificationtime != 0 || accesstime != 0) && beast_stat (fullpath, info))
    {
        struct utimbuf times;
        times.actime  = accesstime != 0       ? (time_t) (accesstime / 1000)       : info.st_atime;
        times.modtime = modificationtime != 0 ? (time_t) (modificationtime / 1000) : info.st_mtime;

        return utime (fullpath.toutf8(), &times) == 0;
    }

    return false;
}

bool file::deletefile() const
{
    if (! exists())
        return true;

    if (isdirectory())
        return rmdir (fullpath.toutf8()) == 0;

    return remove (fullpath.toutf8()) == 0;
}

bool file::moveinternal (const file& dest) const
{
    if (rename (fullpath.toutf8(), dest.getfullpathname().toutf8()) == 0)
        return true;

    if (haswriteaccess() && copyinternal (dest))
    {
        if (deletefile())
            return true;

        dest.deletefile();
    }

    return false;
}

result file::createdirectoryinternal (const string& filename) const
{
    return getresultforreturnvalue (mkdir (filename.toutf8(), 0777));
}

//=====================================================================
std::int64_t beast_filesetposition (void* handle, std::int64_t pos)
{
    if (handle != 0 && lseek (getfd (handle), pos, seek_set) == pos)
        return pos;

    return -1;
}

void fileinputstream::openhandle()
{
    const int f = open (file.getfullpathname().toutf8(), o_rdonly, 00644);

    if (f != -1)
        filehandle = fdtovoidpointer (f);
    else
        status = getresultforerrno();
}

void fileinputstream::closehandle()
{
    if (filehandle != 0)
    {
        close (getfd (filehandle));
        filehandle = 0;
    }
}

size_t fileinputstream::readinternal (void* const buffer, const size_t numbytes)
{
    std::ptrdiff_t result = 0;

    if (filehandle != 0)
    {
        result = ::read (getfd (filehandle), buffer, numbytes);

        if (result < 0)
        {
            status = getresultforerrno();
            result = 0;
        }
    }

    return (size_t) result;
}

//==============================================================================
void fileoutputstream::openhandle()
{
    if (file.exists())
    {
        const int f = open (file.getfullpathname().toutf8(), o_rdwr, 00644);

        if (f != -1)
        {
            currentposition = lseek (f, 0, seek_end);

            if (currentposition >= 0)
            {
                filehandle = fdtovoidpointer (f);
            }
            else
            {
                status = getresultforerrno();
                close (f);
            }
        }
        else
        {
            status = getresultforerrno();
        }
    }
    else
    {
        const int f = open (file.getfullpathname().toutf8(), o_rdwr + o_creat, 00644);

        if (f != -1)
            filehandle = fdtovoidpointer (f);
        else
            status = getresultforerrno();
    }
}

void fileoutputstream::closehandle()
{
    if (filehandle != 0)
    {
        close (getfd (filehandle));
        filehandle = 0;
    }
}

std::ptrdiff_t fileoutputstream::writeinternal (const void* const data, const size_t numbytes)
{
    std::ptrdiff_t result = 0;

    if (filehandle != 0)
    {
        result = ::write (getfd (filehandle), data, numbytes);

        if (result == -1)
            status = getresultforerrno();
    }

    return result;
}

void fileoutputstream::flushinternal()
{
    if (filehandle != 0)
    {
        if (fsync (getfd (filehandle)) == -1)
            status = getresultforerrno();

       #if beast_android
        // this stuff tells the os to asynchronously update the metadata
        // that the os has cached aboud the file - this metadata is used
        // when the device is acting as a usb drive, and unless it's explicitly
        // refreshed, it'll get out of step with the real file.
        const localref<jstring> t (javastring (file.getfullpathname()));
        android.activity.callvoidmethod (beastappactivity.scanfile, t.get());
       #endif
    }
}

result fileoutputstream::truncate()
{
    if (filehandle == 0)
        return status;

    flush();
    return getresultforreturnvalue (ftruncate (getfd (filehandle), (off_t) currentposition));
}

//==============================================================================
#if beast_probeastr_live_build
extern "c" const char* beast_getcurrentexecutablepath();
#endif

file beast_getexecutablefile();
file beast_getexecutablefile()
{
   #if beast_probeastr_live_build
    return file (beast_getcurrentexecutablepath());
   #elif beast_android
    return file (android.appfile);
   #else
    struct dladdrreader
    {
        static string getfilename()
        {
            dl_info exeinfo;
            dladdr ((void*) beast_getexecutablefile, &exeinfo);
            return charpointer_utf8 (exeinfo.dli_fname);
        }
    };

    static string filename (dladdrreader::getfilename());
    return file::getcurrentworkingdirectory().getchildfile (filename);
   #endif
}

//==============================================================================
std::int64_t file::getbytesfreeonvolume() const
{
    struct statfs buf;
    if (beast_dostatfs (*this, buf))
        return (std::int64_t) buf.f_bsize * (std::int64_t) buf.f_bavail; // note: this returns space available to non-super user

    return 0;
}

std::int64_t file::getvolumetotalsize() const
{
    struct statfs buf;
    if (beast_dostatfs (*this, buf))
        return (std::int64_t) buf.f_bsize * (std::int64_t) buf.f_blocks;

    return 0;
}

} // beast
