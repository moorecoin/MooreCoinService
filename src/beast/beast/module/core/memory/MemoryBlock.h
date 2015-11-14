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

#ifndef beast_memoryblock_h_included
#define beast_memoryblock_h_included

#include <beast/heapblock.h>
#include <beast/utility/leakchecked.h>
#include <beast/strings/string.h>

namespace beast {

//==============================================================================
/**
    a class to hold a resizable block of raw data.

*/
class memoryblock : leakchecked <memoryblock>
{
public:
    //==============================================================================
    /** create an uninitialised block with 0 size. */
    memoryblock() noexcept;

    /** creates a memory block with a given initial size.

        @param initialsize          the size of block to create
        @param initialisetozero     whether to clear the memory or just leave it uninitialised
    */
    memoryblock (const size_t initialsize,
                 bool initialisetozero = false);

    /** creates a copy of another memory block. */
    memoryblock (const memoryblock& other);

    /** creates a memory block using a copy of a block of data.

        @param datatoinitialisefrom     some data to copy into this block
        @param sizeinbytes              how much space to use
    */
    memoryblock (const void* datatoinitialisefrom, size_t sizeinbytes);

    /** destructor. */
    ~memoryblock() noexcept;

    /** copies another memory block onto this one.

        this block will be resized and copied to exactly match the other one.
    */
    memoryblock& operator= (const memoryblock& other);

   #if beast_compiler_supports_move_semantics
    memoryblock (memoryblock&& other) noexcept;
    memoryblock& operator= (memoryblock&& other) noexcept;
   #endif

    // standard container interface
    typedef char* iterator;
    typedef char const* const_iterator;
    inline iterator begin () noexcept { return static_cast <iterator> (getdata ()); }
    inline iterator end () noexcept { return addbytestopointer (begin (), size); }
    inline const_iterator cbegin () const noexcept { return static_cast <const_iterator> (getconstdata ()); }
    inline const_iterator cend () const noexcept { return addbytestopointer (cbegin (), size); }

    //==============================================================================
    /** compares two memory blocks.

        @returns true only if the two blocks are the same size and have identical contents.
    */
    bool operator== (const memoryblock& other) const noexcept;

    /** compares two memory blocks.

        @returns true if the two blocks are different sizes or have different contents.
    */
    bool operator!= (const memoryblock& other) const noexcept;

    /** returns true if the data in this memoryblock matches the raw bytes passed-in.
    */
    bool matches (const void* data, size_t datasize) const noexcept;

    //==============================================================================
    /** returns a void pointer to the data.

        note that the pointer returned will probably become invalid when the
        block is resized.
    */
    void* getdata() const noexcept
    {
        return data;
    }

    /** returns a void pointer to data as unmodifiable.

        note that the pointer returned will probably become invalid when the
        block is resized.
    */
    void const* getconstdata() const noexcept
    {
        return data;
    }

    /** returns a byte from the memory block.

        this returns a reference, so you can also use it to set a byte.
    */
    template <typename type>
    char& operator[] (const type offset) const noexcept             { return data [offset]; }

    //==============================================================================
    /** returns the block's current allocated size, in bytes. */
    size_t getsize() const noexcept                                 { return size; }

    /** resizes the memory block.

        this will try to keep as much of the block's current content as it can,
        and can optionally be made to clear any new space that gets allocated at
        the end of the block.

        @param newsize                      the new desired size for the block
        @param initialisenewspacetozero     if the block gets enlarged, this determines
                                            whether to clear the new section or just leave it
                                            uninitialised
        @see ensuresize
    */
    void setsize (const size_t newsize,
                  bool initialisenewspacetozero = false);

    /** increases the block's size only if it's smaller than a given size.

        @param minimumsize                  if the block is already bigger than this size, no action
                                            will be taken; otherwise it will be increased to this size
        @param initialisenewspacetozero     if the block gets enlarged, this determines
                                            whether to clear the new section or just leave it
                                            uninitialised
        @see setsize
    */
    void ensuresize (const size_t minimumsize,
                     bool initialisenewspacetozero = false);

    //==============================================================================
    /** fills the entire memory block with a repeated byte value.

        this is handy for clearing a block of memory to zero.
    */
    void fillwith (std::uint8_t valuetouse) noexcept;

    /** adds another block of data to the end of this one.
        the data pointer must not be null. this block's size will be increased accordingly.
    */
    void append (const void* data, size_t numbytes);

    /** resizes this block to the given size and fills its contents from the supplied buffer.
        the data pointer must not be null.
    */
    void replacewith (const void* data, size_t numbytes);

    /** inserts some data into the block.
        the datatoinsert pointer must not be null. this block's size will be increased accordingly.
        if the insert position lies outside the valid range of the block, it will be clipped to
        within the range before being used.
    */
    void insert (const void* datatoinsert, size_t numbytestoinsert, size_t insertposition);

    /** chops out a section  of the block.

        this will remove a section of the memory block and close the gap around it,
        shifting any subsequent data downwards and reducing the size of the block.

        if the range specified goes beyond the size of the block, it will be clipped.
    */
    void removesection (size_t startbyte, size_t numbytestoremove);

    //==============================================================================
    /** copies data into this memoryblock from a memory address.

        @param srcdata              the memory location of the data to copy into this block
        @param destinationoffset    the offset in this block at which the data being copied should begin
        @param numbytes             how much to copy in (if this goes beyond the size of the memory block,
                                    it will be clipped so not to do anything nasty)
    */
    void copyfrom (const void* srcdata,
                   int destinationoffset,
                   size_t numbytes) noexcept;

    /** copies data from this memoryblock to a memory address.

        @param destdata         the memory location to write to
        @param sourceoffset     the offset within this block from which the copied data will be read
        @param numbytes         how much to copy (if this extends beyond the limits of the memory block,
                                zeros will be used for that portion of the data)
    */
    void copyto (void* destdata,
                 int sourceoffset,
                 size_t numbytes) const noexcept;

    //==============================================================================
    /** exchanges the contents of this and another memory block.
        no actual copying is required for this, so it's very fast.
    */
    void swapwith (memoryblock& other) noexcept;

    //==============================================================================
    /** attempts to parse the contents of the block as a zero-terminated utf8 string. */
    string tostring() const;

    //==============================================================================
    /** parses a string of hexadecimal numbers and writes this data into the memory block.

        the block will be resized to the number of valid bytes read from the string.
        non-hex characters in the string will be ignored.

        @see string::tohexstring()
    */
    void loadfromhexstring (const string& sourcehexstring);

    //==============================================================================
    /** sets a number of bits in the memory block, treating it as a long binary sequence. */
    void setbitrange (size_t bitrangestart,
                      size_t numbits,
                      int binarynumbertoapply) noexcept;

    /** reads a number of bits from the memory block, treating it as one long binary sequence */
    int getbitrange (size_t bitrangestart,
                     size_t numbitstoread) const noexcept;

    //==============================================================================
    /** returns a string of characters that represent the binary contents of this block.

        uses a 64-bit encoding system to allow binary data to be turned into a string
        of simple non-extended characters, e.g. for storage in xml.

        @see frombase64encoding
    */
    string tobase64encoding() const;

    /** takes a string of encoded characters and turns it into binary data.

        the string passed in must have been created by to64bitencoding(), and this
        block will be resized to recreate the original data block.

        @see tobase64encoding
    */
    bool frombase64encoding  (const string& encodedstring);


private:
    //==============================================================================
    heapblock <char> data;
    size_t size;
};

} // beast

#endif

