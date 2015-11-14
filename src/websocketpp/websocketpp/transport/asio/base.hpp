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

#ifndef websocketpp_transport_asio_base_hpp
#define websocketpp_transport_asio_base_hpp

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/system_error.hpp>

#include <boost/system/error_code.hpp>

#include <boost/aligned_storage.hpp>
#include <boost/noncopyable.hpp>
#include <boost/array.hpp>

#include <string>

namespace websocketpp {
namespace transport {
/// transport policy that uses boost::asio
/**
 * this policy uses a single boost::asio io_service to provide transport
 * services to a websocket++ endpoint.
 */
namespace asio {

//

// class to manage the memory to be used for handler-based custom allocation.
// it contains a single block of memory which may be returned for allocation
// requests. if the memory is in use when an allocation request is made, the
// allocator delegates allocation to the global heap.
class handler_allocator
  : private boost::noncopyable
{
public:
  handler_allocator()
    : in_use_(false)
  {
  }

  void* allocate(std::size_t size)
  {
    if (!in_use_ && size < storage_.size)
    {
      in_use_ = true;
      return storage_.address();
    }
    else
    {
      return ::operator new(size);
    }
  }

  void deallocate(void* pointer)
  {
    if (pointer == storage_.address())
    {
      in_use_ = false;
    }
    else
    {
      ::operator delete(pointer);
    }
  }

private:
  // storage space used for handler-based custom memory allocation.
  boost::aligned_storage<1024> storage_;

  // whether the handler-based custom allocation storage has been used.
  bool in_use_;
};

// wrapper class template for handler objects to allow handler memory
// allocation to be customised. calls to operator() are forwarded to the
// encapsulated handler.
template <typename handler>
class custom_alloc_handler
{
public:
  custom_alloc_handler(handler_allocator& a, handler h)
    : allocator_(a),
      handler_(h)
  {
  }

  template <typename arg1>
  void operator()(arg1 arg1)
  {
    handler_(arg1);
  }

  template <typename arg1, typename arg2>
  void operator()(arg1 arg1, arg2 arg2)
  {
    handler_(arg1, arg2);
  }

  friend void* asio_handler_allocate(std::size_t size,
      custom_alloc_handler<handler>* this_handler)
  {
    return this_handler->allocator_.allocate(size);
  }

  friend void asio_handler_deallocate(void* pointer, std::size_t /*size*/,
      custom_alloc_handler<handler>* this_handler)
  {
    this_handler->allocator_.deallocate(pointer);
  }

private:
  handler_allocator& allocator_;
  handler handler_;
};

// helper function to wrap a handler object to add custom allocation.
template <typename handler>
inline custom_alloc_handler<handler> make_custom_alloc_handler(
    handler_allocator& a, handler h)
{
  return custom_alloc_handler<handler>(a, h);
}







// forward declaration of class endpoint so that it can be friended/referenced
// before being included.
template <typename config>
class endpoint;

typedef lib::function<void(boost::system::error_code const &)>
    socket_shutdown_handler;

typedef lib::function<void (boost::system::error_code const & ec,
    size_t bytes_transferred)> async_read_handler;

typedef lib::function<void (boost::system::error_code const & ec,
    size_t bytes_transferred)> async_write_handler;

typedef lib::function<void (lib::error_code const & ec)> pre_init_handler;

// handle_timer: dynamic parameters, multiple copies
// handle_proxy_write
// handle_proxy_read
// handle_async_write
// handle_pre_init


/// asio transport errors
namespace error {
enum value {
    /// catch-all error for transport policy errors that don't fit in other
    /// categories
    general = 1,

    /// async_read_at_least call requested more bytes than buffer can store
    invalid_num_bytes,

    /// there was an error in the underlying transport library
    pass_through,

    /// the connection to the requested proxy server failed
    proxy_failed,

    /// invalid proxy uri
    proxy_invalid,

    /// invalid host or service
    invalid_host_service
};

/// asio transport error category
class category : public lib::error_category {
public:
    char const * name() const _websocketpp_noexcept_token_ {
        return "websocketpp.transport.asio";
    }

    std::string message(int value) const {
        switch(value) {
            case error::general:
                return "generic asio transport policy error";
            case error::invalid_num_bytes:
                return "async_read_at_least call requested more bytes than buffer can store";
            case error::pass_through:
                return "underlying transport error";
            case error::proxy_failed:
                return "proxy connection failed";
            case error::proxy_invalid:
                return "invalid proxy uri";
            case error::invalid_host_service:
                return "invalid host or service";
            default:
                return "unknown";
        }
    }
};

/// get a reference to a static copy of the asio transport error category
inline lib::error_category const & get_category() {
    static category instance;
    return instance;
}

/// create an error code with the given value and the asio transport category
inline lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace asio
} // namespace transport
} // namespace websocketpp

_websocketpp_error_code_enum_ns_start_
template<> struct is_error_code_enum<websocketpp::transport::asio::error::value>
{
    static bool const value = true;
};
_websocketpp_error_code_enum_ns_end_
#endif // websocketpp_transport_asio_hpp
