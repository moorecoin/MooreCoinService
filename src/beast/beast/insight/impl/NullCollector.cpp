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

namespace beast {
namespace insight {

namespace detail {

class nullhookimpl : public hookimpl
{
private:
    nullhookimpl& operator= (nullhookimpl const&);
};

//------------------------------------------------------------------------------

class nullcounterimpl : public counterimpl
{
public:
    void increment (value_type)
    {
    }

private:
    nullcounterimpl& operator= (nullcounterimpl const&);
};

//------------------------------------------------------------------------------

class nulleventimpl : public eventimpl
{
public:
    void notify (value_type const&)
    {
    }

private:
    nulleventimpl& operator= (nulleventimpl const&);
};

//------------------------------------------------------------------------------

class nullgaugeimpl : public gaugeimpl
{
public:
    void set (value_type)
    {
    }

    void increment (difference_type)
    {
    }

private:
    nullgaugeimpl& operator= (nullgaugeimpl const&);
};

//------------------------------------------------------------------------------

class nullmeterimpl : public meterimpl
{
public:
    void increment (value_type)
    {
    }

private:
    nullmeterimpl& operator= (nullmeterimpl const&);
};

//------------------------------------------------------------------------------

class nullcollectorimp : public nullcollector
{
private:
public:
    nullcollectorimp ()
    {
    }

    ~nullcollectorimp ()
    {
    }

    hook make_hook (hookimpl::handlertype const&)
    {
        return hook (std::make_shared <detail::nullhookimpl> ());
    }

    counter make_counter (std::string const&)
    {
        return counter (std::make_shared <detail::nullcounterimpl> ());
    }

    event make_event (std::string const&)
    {
        return event (std::make_shared <detail::nulleventimpl> ());
    }

    gauge make_gauge (std::string const&)
    {
        return gauge (std::make_shared <detail::nullgaugeimpl> ());
    }
    
    meter make_meter (std::string const&)
    {
        return meter (std::make_shared <detail::nullmeterimpl> ());
    }
};

}

//------------------------------------------------------------------------------

std::shared_ptr <collector> nullcollector::new ()
{
    return std::make_shared <detail::nullcollectorimp> ();
}

}
}
