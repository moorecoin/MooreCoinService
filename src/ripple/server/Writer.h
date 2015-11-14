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

#ifndef ripple_server_writer_h_included
#define ripple_server_writer_h_included

#include <boost/asio/buffer.hpp>
#include <functional>
#include <vector>

namespace ripple {
namespace http {

class writer
{
public:
    virtual ~writer() = default;

    /** returns `true` if there is no more data to pull. */
    virtual
    bool
    complete() = 0;

    /** removes bytes from the input sequence. */
    virtual
    void
    consume (std::size_t bytes) = 0;

    /** add data to the input sequence.
        @param bytes a hint to the number of bytes desired.
        @param resume a functor to later resume execution.
        @return `true` if the writer is ready to provide more data.
    */
    virtual
    bool
    prepare (std::size_t bytes,
        std::function<void(void)> resume) = 0;

    /** returns a constbuffersequence representing the input sequence. */
    virtual
    std::vector<boost::asio::const_buffer>
    data() = 0;
};

}
}

#endif
