//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_nudb_win32_file_h_included
#define beast_nudb_win32_file_h_included

#include <beast/nudb/common.h>
#include <cassert>
#include <string>

#ifndef beast_nudb_win32_file
# ifdef _msc_ver
#  define beast_nudb_win32_file 1
# else
#  define beast_nudb_win32_file 0
# endif
#endif

#if beast_nudb_win32_file
# ifndef nominmax
#  define nominmax
# endif
# ifndef unicode
#  define unicode
# endif
# ifndef strict
#  define strict
# endif
# include <windows.h>
# undef nominmax
# undef unicode
# undef strict
#endif

namespace beast {
namespace nudb {

#if beast_nudb_win32_file

namespace detail {

// win32 error code
class file_win32_error
    : public file_error
{
public:
    explicit
    file_win32_error (char const* m,
            dword dwerror = ::getlasterror())
        : file_error (std::string("nudb: ") + m +
            ", " + text(dwerror))
    {
    }

    explicit
    file_win32_error (std::string const& m,
            dword dwerror = ::getlasterror())
        : file_error (std::string("nudb: ") + m +
            ", " + text(dwerror))
    {
    }

private:
    template <class = void>
    static
    std::string
    text (dword dwerror);
};

template <class>
std::string
file_win32_error::text (dword dwerror)
{
    lpstr buf = nullptr;
    size_t const size = formatmessagea (
        format_message_allocate_buffer |
        format_message_from_system |
        format_message_ignore_inserts,
        null,
        dwerror,
        makelangid(lang_neutral, sublang_default),
        (lpstr)&buf,
        0,
        null);
    std::string s;
    if (size)
    {
        s.append(buf, size);
        localfree (buf);
    }
    else
    {
        s = "error " + std::to_string(dwerror);
    }
    return s;
}

//------------------------------------------------------------------------------

template <class = void>
class win32_file
{
private:
    handle hf_ = invalid_handle_value;

public:
    win32_file() = default;
    win32_file (win32_file const&) = delete;
    win32_file& operator= (win32_file const&) = delete;

    ~win32_file();

    win32_file (win32_file&&);

    win32_file&
    operator= (win32_file&& other);

    bool
    is_open() const
    {
        return hf_ != invalid_handle_value;
    }

    void
    close();

    //  returns:
    //      `false` if the file already exists
    //      `true` on success, else throws
    //
    bool
    create (file_mode mode, std::string const& path);

    //  returns:
    //      `false` if the file doesnt exist
    //      `true` on success, else throws
    //
    bool
    open (file_mode mode, std::string const& path);

    //  effects:
    //      removes the file from the file system.
    //
    //  throws:
    //      throws is an error occurs.
    //
    //  returns:
    //      `true` if the file was erased
    //      `false` if the file was not present
    //
    static
    bool
    erase (path_type const& path);

    //  returns:
    //      current file size in bytes measured by operating system
    //  requires:
    //      is_open() == true
    //
    std::size_t
    actual_size() const;

    void
    read (std::size_t offset,
        void* buffer, std::size_t bytes);

    void
    write (std::size_t offset,
        void const* buffer, std::size_t bytes);

    void
    sync();

    void
    trunc (std::size_t length);

private:
    static
    std::pair<dword, dword>
    flags (file_mode mode);
};

template <class _>
win32_file<_>::~win32_file()
{
    close();
}

template <class _>
win32_file<_>::win32_file (win32_file&& other)
    : hf_ (other.hf_)
{
    other.hf_ = invalid_handle_value;
}

template <class _>
win32_file<_>&
win32_file<_>::operator= (win32_file&& other)
{
    if (&other == this)
        return *this;
    close();
    hf_ = other.hf_;
    other.hf_ = invalid_handle_value;
    return *this;
}

template <class _>
void
win32_file<_>::close()
{
    if (hf_ != invalid_handle_value)
    {
        ::closehandle(hf_);
        hf_ = invalid_handle_value;
    }
}

template <class _>
bool
win32_file<_>::create (file_mode mode,
    std::string const& path)
{
    assert(! is_open());
    auto const f = flags(mode);
    hf_ = ::createfilea (path.c_str(),
        f.first,
        0,
        null,
        create_new,
        f.second,
        null);
    if (hf_ == invalid_handle_value)
    {
        dword const dwerror = ::getlasterror();
        if (dwerror != error_file_exists)
            throw file_win32_error(
                "create file", dwerror);
        return false;
    }
    return true;
}

template <class _>
bool
win32_file<_>::open (file_mode mode,
    std::string const& path)
{
    assert(! is_open());
    auto const f = flags(mode);
    hf_ = ::createfilea (path.c_str(),
        f.first,
        0,
        null,
        open_existing,
        f.second,
        null);
    if (hf_ == invalid_handle_value)
    {
        dword const dwerror = ::getlasterror();
        if (dwerror != error_file_not_found &&
            dwerror != error_path_not_found)
            throw file_win32_error(
                "open file", dwerror);
        return false;
    }
    return true;
}

template <class _>
bool
win32_file<_>::erase (path_type const& path)
{
    bool const bsuccess =
        ::deletefilea(path.c_str());
    if (! bsuccess)
    {
        dword dwerror = ::getlasterror();
        if (dwerror != error_file_not_found &&
            dwerror != error_path_not_found)
            throw file_win32_error(
                "erase file");
        return false;
    }
    return true;
}

// return: current file size in bytes measured by operating system
template <class _>
std::size_t
win32_file<_>::actual_size() const
{
    assert(is_open());
    large_integer filesize;
    if (! ::getfilesizeex(hf_, &filesize))
        throw file_win32_error(
            "size file");
    return static_cast<std::size_t>(filesize.quadpart);
}

template <class _>
void
win32_file<_>::read (std::size_t offset,
    void* buffer, std::size_t bytes)
{
    while(bytes > 0)
    {
        dword bytesread;
        large_integer li;
        li.quadpart = static_cast<longlong>(offset);
        overlapped ov;
        ov.offset = li.lowpart;
        ov.offsethigh = li.highpart;
        ov.hevent = null;
        bool const bsuccess = ::readfile(
            hf_, buffer, bytes, &bytesread, &ov);
        if (! bsuccess)
        {
            dword const dwerror = ::getlasterror();
            if (dwerror != error_handle_eof)
                throw file_win32_error(
                    "read file", dwerror);
            throw file_short_read_error();
        }
        if (bytesread == 0)
            throw file_short_read_error();
        offset += bytesread;
        bytes -= bytesread;
        buffer = reinterpret_cast<char*>(
            buffer) + bytesread;
    }
}

template <class _>
void
win32_file<_>::write (std::size_t offset,
    void const* buffer, std::size_t bytes)
{
    while(bytes > 0)
    {
        large_integer li;
        li.quadpart = static_cast<longlong>(offset);
        overlapped ov;
        ov.offset = li.lowpart;
        ov.offsethigh = li.highpart;
        ov.hevent = null;
        dword byteswritten;
        bool const bsuccess = ::writefile(
            hf_, buffer, bytes, &byteswritten, &ov);
        if (! bsuccess)
            throw file_win32_error(
                "write file");
        if (byteswritten == 0)
            throw file_short_write_error();
        offset += byteswritten;
        bytes -= byteswritten;
        buffer = reinterpret_cast<
            char const*>(buffer) +
                byteswritten;
    }
}

template <class _>
void
win32_file<_>::sync()
{
    bool const bsuccess =
        ::flushfilebuffers(hf_);
    if (! bsuccess)
        throw file_win32_error(
            "sync file");
}

template <class _>
void
win32_file<_>::trunc (std::size_t length)
{
    large_integer li;
    li.quadpart = length;
    bool bsuccess;
    bsuccess = ::setfilepointerex(
        hf_, li, null, file_begin);
    if (bsuccess)
        bsuccess = setendoffile(hf_);
    if (! bsuccess)
        throw file_win32_error(
            "trunc file");
}

template <class _>
std::pair<dword, dword>
win32_file<_>::flags (file_mode mode)
{
    std::pair<dword, dword> result(0, 0);
    switch (mode)
    {
    case file_mode::scan:
        result.first =
            generic_read;
        result.second =
            file_flag_sequential_scan;
        break;

    case file_mode::read:
        result.first =
            generic_read;
        result.second =
            file_flag_random_access;
        break;

    case file_mode::append:
        result.first =
            generic_read | generic_write;
        result.second =
            file_flag_random_access
            //| file_flag_no_buffering
            //| file_flag_write_through
            ;
        break;

    case file_mode::write:
        result.first =
            generic_read | generic_write;
        result.second =
            file_flag_random_access;
        break;
    }
    return result;
}

} // detail

using win32_file = detail::win32_file<>;

#endif

} // nudb
} // detail

#endif
