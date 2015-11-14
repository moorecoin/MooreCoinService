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

#ifndef beast_nudb_posix_file_h_included
#define beast_nudb_posix_file_h_included

#include <beast/nudb/common.h>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <string>
#include <utility>

#ifndef beast_nudb_posix_file
# ifdef _msc_ver
#  define beast_nudb_posix_file 0
# else
#  define beast_nudb_posix_file 1
# endif
#endif

#if beast_nudb_posix_file
# include <fcntl.h>
# include <sys/types.h>
# include <sys/uio.h>
# include <sys/stat.h>
# include <unistd.h>
#endif

namespace beast {
namespace nudb {

#if beast_nudb_posix_file

namespace detail {

class file_posix_error : public file_error
{
public:
    explicit
    file_posix_error (char const* m,
        int errnum = errno)
        : file_error (std::string("nudb: ") + m +
            ", " + text(errnum))
    {
    }

    explicit
    file_posix_error (std::string const& m,
        int errnum = errno)
        : file_error (std::string("nudb: ") + m +
            ", " + text(errnum))
    {
    }

private:
    static
    std::string
    text (int errnum)
    {
        return ::strerror(errnum);
    }
};

//------------------------------------------------------------------------------

template <class = void>
class posix_file
{
private:
    int fd_ = -1;

public:
    posix_file() = default;
    posix_file (posix_file const&) = delete;
    posix_file& operator= (posix_file const&) = delete;

    ~posix_file();

    posix_file (posix_file&&);

    posix_file&
    operator= (posix_file&& other);

    bool
    is_open() const
    {
        return fd_ != -1;
    }

    void
    close();

    bool
    create (file_mode mode, path_type const& path);

    bool
    open (file_mode mode, path_type const& path);

    static
    bool
    erase (path_type const& path);

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
    std::pair<int, int>
    flags (file_mode mode);
};

template <class _>
posix_file<_>::~posix_file()
{
    close();
}

template <class _>
posix_file<_>::posix_file (posix_file &&other)
    : fd_ (other.fd_)
{
    other.fd_ = -1;
}

template <class _>
posix_file<_>&
posix_file<_>::operator= (posix_file&& other)
{
    if (&other == this)
        return *this;
    close();
    fd_ = other.fd_;
    other.fd_ = -1;
    return *this;
}

template <class _>
void
posix_file<_>::close()
{
    if (fd_ != -1)
    {
        if (::close(fd_) != 0)
            throw file_posix_error(
                "close file");
        fd_ = -1;
    }
}

template <class _>
bool
posix_file<_>::create (file_mode mode,
    path_type const& path)
{
    auto const result = flags(mode);
    assert(! is_open());
    fd_ = ::open(path.c_str(), result.first);
    if (fd_ != -1)
    {
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    int errnum = errno;
    if (errnum != enoent)
        throw file_posix_error(
            "open file", errnum);
    fd_ = ::open(path.c_str(),
        result.first | o_creat, 0644);
    if (fd_ == -1)
        throw file_posix_error(
            "create file");
#ifndef __apple__
    if (::posix_fadvise(fd_, 0, 0, result.second) != 0)
        throw file_posix_error(
            "fadvise");
#endif
    return true;
}

template <class _>
bool
posix_file<_>::open (file_mode mode,
    path_type const& path)
{
    assert(! is_open());
    auto const result = flags(mode);
    fd_ = ::open(path.c_str(), result.first);
    if (fd_ == -1)
    {
        int errnum = errno;
        if (errnum == enoent)
            return false;
        throw file_posix_error(
            "open file", errnum);
    }
#ifndef __apple__
    if (::posix_fadvise(fd_, 0, 0, result.second) != 0)
        throw file_posix_error(
            "fadvise");
#endif
    return true;
}

template <class _>
bool
posix_file<_>::erase (path_type const& path)
{
    if (::unlink(path.c_str()) != 0)
    {
        int const ec = errno;
        if (ec != enoent)
            throw file_posix_error(
                "unlink", ec);
        return false;
    }
    return true;
}

template <class _>
std::size_t
posix_file<_>::actual_size() const
{
    struct stat st;
    if (::fstat(fd_, &st) != 0)
        throw file_posix_error(
            "fstat");
    return st.st_size;
}

template <class _>
void
posix_file<_>::read (std::size_t offset,
    void* buffer, std::size_t bytes)
{
    while(bytes > 0)
    {
        auto const n = ::pread (
            fd_, buffer, bytes, offset);
        // vfalco end of file should throw short_read
        if (n == -1)
            throw file_posix_error(
                "pread");
        if (n == 0)
            throw file_short_read_error();
        offset += n;
        bytes -= n;
        buffer = reinterpret_cast<
            char*>(buffer) + n;
    }
}

template <class _>
void
posix_file<_>::write (std::size_t offset,
    void const* buffer, std::size_t bytes)
{
    while(bytes > 0)
    {
        auto const n = ::pwrite (
            fd_, buffer, bytes, offset);
        if (n == -1)
            throw file_posix_error(
                "pwrite");
        if (n == 0)
            throw file_short_write_error();
        offset += n;
        bytes -= n;
        buffer = reinterpret_cast<
            char const*>(buffer) + n;
    }
}

template <class _>
void
posix_file<_>::sync()
{
    if (::fsync(fd_) != 0)
        throw file_posix_error(
            "fsync");
}

template <class _>
void
posix_file<_>::trunc (std::size_t length)
{
    if (::ftruncate(fd_, length) != 0)
        throw file_posix_error(
            "ftruncate");
}

template <class _>
std::pair<int, int>
posix_file<_>::flags (file_mode mode)
{
    std::pair<int, int> result;
    switch(mode)
    {
    case file_mode::scan:
        result.first =
            o_rdonly;
#ifndef __apple__
        result.second =
            posix_fadv_sequential;
#endif
        break;
    case file_mode::read:
        result.first =
            o_rdonly;
#ifndef __apple__
        result.second =
            posix_fadv_random;
#endif
        break;
    case file_mode::append:
        result.first =
            o_rdwr |
            o_append;
#ifndef __apple__
        result.second =
            posix_fadv_random;
#endif
        break;
    case file_mode::write:
        result.first =
            o_rdwr;
#ifndef __apple__
        result.second =
            posix_fadv_normal;
#endif
        break;
    }
    return result;
}

} // detail

using posix_file = detail::posix_file<>;

#endif

} // nudb
} // beast

#endif
