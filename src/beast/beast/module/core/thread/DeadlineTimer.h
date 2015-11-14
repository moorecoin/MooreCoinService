//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_deadlinetimer_h_included
#define beast_deadlinetimer_h_included

#include <beast/module/core/memory/sharedsingleton.h>

namespace beast {

/** provides periodic or one time notifications at a specified time interval.
*/
class deadlinetimer
    : public list <deadlinetimer>::node
{
public:
    /** listener for a deadline timer.

        the listener is called on an auxiliary thread. it is suggested
        not to perform any time consuming operations during the call.
    */
    // vfalco todo perhaps allow construction using a servicequeue to use
    //             for notifications.
    //
    class listener
    {
    public:
        virtual void ondeadlinetimer (deadlinetimer&) { }
    };

public:
    /** create a deadline timer with the specified listener attached.
    */
    explicit deadlinetimer (listener* listener);

    deadlinetimer (deadlinetimer const&) = delete;
    deadlinetimer& operator= (deadlinetimer const&) = delete;
    
    ~deadlinetimer ();

    /** cancel all notifications.
        it is okay to call this on an inactive timer.
        @note it is guaranteed that no notifications will occur after this
              function returns.
    */
    void cancel ();

    /** set the timer to go off once in the future.
        if the timer is already active, this will reset it.
        @note if the timer is already active, the old one might go off
              before this function returns.
        @param secondsuntildeadline the number of seconds until the timer
                                    will send a notification. this must be
                                    greater than zero.
    */
    /** @{ */
    void setexpiration (double secondsuntildeadline);

    template <class rep, class period>
    void setexpiration (std::chrono::duration <rep, period> const& amount)
    {
        setexpiration (std::chrono::duration_cast <
            std::chrono::duration <double>> (amount).count ());
    }
    /** @} */

    /** set the timer to go off repeatedly with the specified frequency.
        if the timer is already active, this will reset it.
        @note if the timer is already active, the old one might go off
              before this function returns.
        @param secondsuntildeadline the number of seconds until the timer
                                    will send a notification. this must be
                                    greater than zero.
    */
    void setrecurringexpiration (double secondsuntildeadline);

    /** equality comparison.
        timers are equal if they have the same address.
    */
    inline bool operator== (deadlinetimer const& other) const
    {
        return this == &other;
    }

    /** inequality comparison. */
    inline bool operator!= (deadlinetimer const& other) const
    {
        return this != &other;
    }

private:
    class manager;

    listener* const m_listener;
    sharedptr <sharedsingleton <manager> > m_manager;
    bool m_isactive;
    relativetime m_notificationtime;
    double m_secondsrecurring; // non zero if recurring
};

} // beast

#endif
