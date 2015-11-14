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

#ifndef ripple_resource_consumer_h_included
#define ripple_resource_consumer_h_included

#include <ripple/resource/charge.h>
#include <ripple/resource/disposition.h>

namespace ripple {
namespace resource {

struct entry;
class logic;

/** an endpoint that consumes resources. */
class consumer
{
private:
    friend class logic;
    consumer (logic& logic, entry& entry);

public:
    consumer ();
    ~consumer ();
    consumer (consumer const& other);
    consumer& operator= (consumer const& other);

    /** return a human readable string uniquely identifying this consumer. */
    std::string to_string () const;

    /** returns `true` if this is a privileged endpoint. */
    bool admin () const;

    /** raise the consumer's privilege level to a named endpoint.
        the reference to the original endpoint descriptor is released.
    */
    void elevate (std::string const& name);

    /** returns the current disposition of this consumer.
        this should be checked upon creation to determine if the consumer
        should be disconnected immediately.
    */
    disposition disposition() const;

    /** apply a load charge to the consumer. */
    disposition charge (charge const& fee);

    /** returns `true` if the consumer should be warned.
        this consumes the warning.
    */
    bool warn();

    /** returns `true` if the consumer should be disconnected. */
    bool disconnect();

    /** returns the credit balance representing consumption. */
    int balance();

    // private: retrieve the entry associated with the consumer
    entry& entry();

private:
    logic* m_logic;
    entry* m_entry;
};

std::ostream& operator<< (std::ostream& os, consumer const& v);

}
}

#endif
