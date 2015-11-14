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

#ifndef jsoncpp_batchallocator_h_included
#define jsoncpp_batchallocator_h_included

#ifndef jsoncpp_doc_exclude_implementation

namespace json
{

/* fast memory allocator.
 *
 * this memory allocator allocates memory for a batch of object (specified by
 * the page size, the number of object in each page).
 *
 * it does not allow the destruction of a single object. all the allocated objects
 * can be destroyed at once. the memory can be either released or reused for future
 * allocation.
 *
 * the in-place new operator must be used to construct the object using the pointer
 * returned by allocate.
 */
template < typename allocatedtype
, const unsigned int objectperallocation >
class batchallocator
{
public:
    typedef allocatedtype type;

    batchallocator ( unsigned int objectsperpage = 255 )
        : freehead_ ( 0 )
        , objectsperpage_ ( objectsperpage )
    {
        //      printf( "size: %d => %s\n", sizeof(allocatedtype), typeid(allocatedtype).name() );
        assert ( sizeof (allocatedtype) * objectperallocation >= sizeof (allocatedtype*) ); // we must be able to store a slist in the object free space.
        assert ( objectsperpage >= 16 );
        batches_ = allocatebatch ( 0 );  // allocated a dummy page
        currentbatch_ = batches_;
    }

    ~batchallocator ()
    {
        for ( batchinfo* batch = batches_; batch;  )
        {
            batchinfo* nextbatch = batch->next_;
            free ( batch );
            batch = nextbatch;
        }
    }

    /// allocate space for an array of objectperallocation object.
    /// @warning it is the responsability of the caller to call objects constructors.
    allocatedtype* allocate ()
    {
        if ( freehead_ ) // returns node from free list.
        {
            allocatedtype* object = freehead_;
            freehead_ = * (allocatedtype**)object;
            return object;
        }

        if ( currentbatch_->used_ == currentbatch_->end_ )
        {
            currentbatch_ = currentbatch_->next_;

            while ( currentbatch_  &&  currentbatch_->used_ == currentbatch_->end_ )
                currentbatch_ = currentbatch_->next_;

            if ( !currentbatch_  ) // no free batch found, allocate a new one
            {
                currentbatch_ = allocatebatch ( objectsperpage_ );
                currentbatch_->next_ = batches_; // insert at the head of the list
                batches_ = currentbatch_;
            }
        }

        allocatedtype* allocated = currentbatch_->used_;
        currentbatch_->used_ += objectperallocation;
        return allocated;
    }

    /// release the object.
    /// @warning it is the responsability of the caller to actually destruct the object.
    void release ( allocatedtype* object )
    {
        assert ( object != 0 );
        * (allocatedtype**)object = freehead_;
        freehead_ = object;
    }

private:
    struct batchinfo
    {
        batchinfo* next_;
        allocatedtype* used_;
        allocatedtype* end_;
        allocatedtype buffer_[objectperallocation];
    };

    // disabled copy constructor and assignement operator.
    batchallocator ( const batchallocator& );
    void operator = ( const batchallocator&);

    static batchinfo* allocatebatch ( unsigned int objectsperpage )
    {
        const unsigned int mallocsize = sizeof (batchinfo) - sizeof (allocatedtype) * objectperallocation
                                        + sizeof (allocatedtype) * objectperallocation * objectsperpage;
        batchinfo* batch = static_cast<batchinfo*> ( malloc ( mallocsize ) );
        batch->next_ = 0;
        batch->used_ = batch->buffer_;
        batch->end_ = batch->buffer_ + objectsperpage;
        return batch;
    }

    batchinfo* batches_;
    batchinfo* currentbatch_;
    /// head of a single linked list within the allocated space of freeed object
    allocatedtype* freehead_;
    unsigned int objectsperpage_;
};


} // namespace json

# endif // ifndef jsoncpp_doc_include_implementation

#endif // jsoncpp_batchallocator_h_included

