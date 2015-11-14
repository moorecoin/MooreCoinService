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

#ifndef beast_inputsource_h_included
#define beast_inputsource_h_included

namespace beast
{

//==============================================================================
/**
    a lightweight object that can create a stream to read some kind of resource.

    this may be used to refer to a file, or some other kind of source, allowing a
    caller to create an input stream that can read from it when required.

    @see fileinputsource
*/
class inputsource : leakchecked <inputsource>
{
public:
    //==============================================================================
    inputsource() noexcept      {}

    /** destructor. */
    virtual ~inputsource()      {}

    //==============================================================================
    /** returns a new inputstream to read this item.

        @returns            an inputstream that the caller will delete, or nullptr if
                            the filename isn't found.
    */
    virtual inputstream* createinputstream() = 0;

    /** returns a new inputstream to read an item, relative.

        @param relateditempath  the relative pathname of the resource that is required
        @returns            an inputstream that the caller will delete, or nullptr if
                            the item isn't found.
    */
    virtual inputstream* createinputstreamfor (const string& relateditempath) = 0;

    /** returns a hash code that uniquely represents this item.
    */
    virtual std::int64_t hashcode() const = 0;
};

} // beast

#endif   // beast_inputsource_h_included
