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

#ifndef beast_fileinputsource_h_included
#define beast_fileinputsource_h_included

namespace beast
{

//==============================================================================
/**
    a type of inputsource that represents a normal file.

    @see inputsource
*/
class fileinputsource
    : public inputsource
    , leakchecked <fileinputsource>
{
public:
    //==============================================================================
    /** creates a fileinputsource for a file.
        if the usefiletimeinhashgeneration parameter is true, then this object's
        hashcode() method will incorporate the file time into its hash code; if
        false, only the file name will be used for the hash.
    */
    fileinputsource (const file& file, bool usefiletimeinhashgeneration = false);

    fileinputsource (fileinputsource const&) = delete;
    fileinputsource& operator= (fileinputsource const&) = delete;

    /** destructor. */
    ~fileinputsource();

    inputstream* createinputstream();
    inputstream* createinputstreamfor (const string& relateditempath);
    std::int64_t hashcode() const;

private:
    //==============================================================================
    const file file;
    bool usefiletimeinhashgeneration;
};

} // beast

#endif   // beast_fileinputsource_h_included
