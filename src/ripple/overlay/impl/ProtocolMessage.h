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

#ifndef ripple_overlay_protocolmessage_h_included
#define ripple_overlay_protocolmessage_h_included

#include "ripple.pb.h"
#include <ripple/overlay/message.h>
#include <ripple/overlay/impl/zerocopystream.h>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/system/error_code.hpp>
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace ripple {

/** returns the name of a protocol message given its type. */
template <class = void>
std::string
protocolmessagename (int type)
{
    switch (type)
    {
    case protocol::mthello:             return "hello";
    case protocol::mtping:              return "ping";
    case protocol::mtproofofwork:       return "proof_of_work";
    case protocol::mtcluster:           return "cluster";
    case protocol::mtget_peers:         return "get_peers";
    case protocol::mtpeers:             return "peers";
    case protocol::mtendpoints:         return "endpoints";
    case protocol::mttransaction:       return "tx";
    case protocol::mtget_ledger:        return "get_ledger";
    case protocol::mtledger_data:       return "ledger_data";
    case protocol::mtpropose_ledger:    return "propose";
    case protocol::mtstatus_change:     return "status";
    case protocol::mthave_set:          return "have_set";
    case protocol::mtvalidation:        return "validation";
    case protocol::mtget_objects:       return "get_objects";
    default:
        break;
    };
    return "unknown";
}

namespace detail {

template <class t, class buffers, class handler>
std::enable_if_t<std::is_base_of<
    ::google::protobuf::message, t>::value,
        boost::system::error_code>
invoke (int type, buffers const& buffers,
    handler& handler)
{
    zerocopyinputstream<buffers> stream(buffers);
    stream.skip(message::kheaderbytes);
    auto const m (std::make_shared<t>());
    if (! m->parsefromzerocopystream(&stream))
        return boost::system::errc::make_error_code(
            boost::system::errc::invalid_argument);
    auto ec = handler.onmessagebegin (type, m);
    if (! ec)
    {
        handler.onmessage (m);
        handler.onmessageend (type, m);
    }
    return ec;
}

}

/** calls the handler for up to one protocol message in the passed buffers.

    if there is insufficient data to produce a complete protocol
    message, zero is returned for the number of bytes consumed.

    @return the number of bytes consumed, or the error code if any.
*/
template <class buffers, class handler>
std::pair <std::size_t, boost::system::error_code>
invokeprotocolmessage (buffers const& buffers, handler& handler)
{
    std::pair<std::size_t,boost::system::error_code> result = { 0, {} };
    boost::system::error_code& ec = result.second;

    auto const type = message::type(buffers);
    if (type == 0)
        return result;
    auto const size = message::kheaderbytes + message::size(buffers);
    if (boost::asio::buffer_size(buffers) < size)
        return result;

    switch (type)
    {
    case protocol::mthello:         ec = detail::invoke<protocol::tmhello> (type, buffers, handler); break;
    case protocol::mtping:          ec = detail::invoke<protocol::tmping> (type, buffers, handler); break;
    case protocol::mtcluster:       ec = detail::invoke<protocol::tmcluster> (type, buffers, handler); break;
    case protocol::mtget_peers:     ec = detail::invoke<protocol::tmgetpeers> (type, buffers, handler); break;
    case protocol::mtpeers:         ec = detail::invoke<protocol::tmpeers> (type, buffers, handler); break;
    case protocol::mtendpoints:     ec = detail::invoke<protocol::tmendpoints> (type, buffers, handler); break;
    case protocol::mttransaction:   ec = detail::invoke<protocol::tmtransaction> (type, buffers, handler); break;
    case protocol::mtget_ledger:    ec = detail::invoke<protocol::tmgetledger> (type, buffers, handler); break;
    case protocol::mtledger_data:   ec = detail::invoke<protocol::tmledgerdata> (type, buffers, handler); break;
    case protocol::mtpropose_ledger:ec = detail::invoke<protocol::tmproposeset> (type, buffers, handler); break;
    case protocol::mtstatus_change: ec = detail::invoke<protocol::tmstatuschange> (type, buffers, handler); break;
    case protocol::mthave_set:      ec = detail::invoke<protocol::tmhavetransactionset> (type, buffers, handler); break;
    case protocol::mtvalidation:    ec = detail::invoke<protocol::tmvalidation> (type, buffers, handler); break;
    case protocol::mtget_objects:   ec = detail::invoke<protocol::tmgetobjectbyhash> (type, buffers, handler); break;
    default:
        ec = handler.onmessageunknown (type);
        break;
    }
    if (! ec)
        result.first = size;

    return result;
}

/** write a protocol message to a streambuf. */
template <class streambuf>
void
write (streambuf& streambuf,
    ::google::protobuf::message const& m, int type,
        std::size_t blockbytes)
{
    auto const size = m.bytesize();
    std::array<std::uint8_t, 6> v;
    v[0] = static_cast<std::uint8_t> ((size >> 24) & 0xff);
    v[1] = static_cast<std::uint8_t> ((size >> 16) & 0xff);
    v[2] = static_cast<std::uint8_t> ((size >>  8) & 0xff);
    v[3] = static_cast<std::uint8_t> ( size        & 0xff);
    v[4] = static_cast<std::uint8_t> ((type >> 8)  & 0xff);
    v[5] = static_cast<std::uint8_t> ( type        & 0xff);

    streambuf.commit(boost::asio::buffer_copy(
        streambuf.prepare(message::kheaderbytes), boost::asio::buffer(v)));
    zerocopyoutputstream<streambuf> stream (
        streambuf, blockbytes);
    m.serializetozerocopystream(&stream);
}

} // ripple

#endif
