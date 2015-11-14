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

// included by json_value.cpp
// everything is within json namespace

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class valueinternalmap
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

/** \internal must be safely initialized using memset( this, 0, sizeof(valueinternallink) );
   * this optimization is used by the fast allocator.
   */
valueinternallink::valueinternallink ()
    : previous_ ( 0 )
    , next_ ( 0 )
{
}

valueinternallink::~valueinternallink ()
{
    for ( int index = 0; index < itemperlink; ++index )
    {
        if ( !items_[index].isitemavailable () )
        {
            if ( !items_[index].ismembernamestatic () )
                free ( keys_[index] );
        }
        else
            break;
    }
}



valuemapallocator::~valuemapallocator ()
{
}

#ifdef json_use_simple_internal_allocator
class defaultvaluemapallocator : public valuemapallocator
{
public: // overridden from valuemapallocator
    virtual valueinternalmap* newmap ()
    {
        return new valueinternalmap ();
    }

    virtual valueinternalmap* newmapcopy ( const valueinternalmap& other )
    {
        return new valueinternalmap ( other );
    }

    virtual void destructmap ( valueinternalmap* map )
    {
        delete map;
    }

    virtual valueinternallink* allocatemapbuckets ( unsigned int size )
    {
        return new valueinternallink[size];
    }

    virtual void releasemapbuckets ( valueinternallink* links )
    {
        delete [] links;
    }

    virtual valueinternallink* allocatemaplink ()
    {
        return new valueinternallink ();
    }

    virtual void releasemaplink ( valueinternallink* link )
    {
        delete link;
    }
};
#else
/// @todo make this thread-safe (lock when accessign batch allocator)
class defaultvaluemapallocator : public valuemapallocator
{
public: // overridden from valuemapallocator
    virtual valueinternalmap* newmap ()
    {
        valueinternalmap* map = mapsallocator_.allocate ();
        new (map) valueinternalmap (); // placement new
        return map;
    }

    virtual valueinternalmap* newmapcopy ( const valueinternalmap& other )
    {
        valueinternalmap* map = mapsallocator_.allocate ();
        new (map) valueinternalmap ( other ); // placement new
        return map;
    }

    virtual void destructmap ( valueinternalmap* map )
    {
        if ( map )
        {
            map->~valueinternalmap ();
            mapsallocator_.release ( map );
        }
    }

    virtual valueinternallink* allocatemapbuckets ( unsigned int size )
    {
        return new valueinternallink[size];
    }

    virtual void releasemapbuckets ( valueinternallink* links )
    {
        delete [] links;
    }

    virtual valueinternallink* allocatemaplink ()
    {
        valueinternallink* link = linksallocator_.allocate ();
        memset ( link, 0, sizeof (valueinternallink) );
        return link;
    }

    virtual void releasemaplink ( valueinternallink* link )
    {
        link->~valueinternallink ();
        linksallocator_.release ( link );
    }
private:
    batchallocator<valueinternalmap, 1> mapsallocator_;
    batchallocator<valueinternallink, 1> linksallocator_;
};
#endif

static valuemapallocator*& mapallocator ()
{
    static defaultvaluemapallocator defaultallocator;
    static valuemapallocator* mapallocator = &defaultallocator;
    return mapallocator;
}

static struct dummymapallocatorinitializer
{
    dummymapallocatorinitializer ()
    {
        mapallocator ();     // ensure mapallocator() statics are initialized before main().
    }
} dummymapallocatorinitializer;



// h(k) = value * k >> w ; with w = 32 & k prime w.r.t. 2^32.

/*
use linked list hash map.
buckets array is a container.
linked list element contains 6 key/values. (memory = (16+4) * 6 + 4 = 124)
value have extra state: valid, available, deleted
*/


valueinternalmap::valueinternalmap ()
    : buckets_ ( 0 )
    , taillink_ ( 0 )
    , bucketssize_ ( 0 )
    , itemcount_ ( 0 )
{
}


valueinternalmap::valueinternalmap ( const valueinternalmap& other )
    : buckets_ ( 0 )
    , taillink_ ( 0 )
    , bucketssize_ ( 0 )
    , itemcount_ ( 0 )
{
    reserve ( other.itemcount_ );
    iteratorstate it;
    iteratorstate itend;
    other.makebeginiterator ( it );
    other.makeenditerator ( itend );

    for ( ; !equals (it, itend); increment (it) )
    {
        bool isstatic;
        const char* membername = key ( it, isstatic );
        const value& avalue = value ( it );
        resolvereference (membername, isstatic) = avalue;
    }
}


valueinternalmap&
valueinternalmap::operator = ( const valueinternalmap& other )
{
    valueinternalmap dummy ( other );
    swap ( dummy );
    return *this;
}


valueinternalmap::~valueinternalmap ()
{
    if ( buckets_ )
    {
        for ( bucketindex bucketindex = 0; bucketindex < bucketssize_; ++bucketindex )
        {
            valueinternallink* link = buckets_[bucketindex].next_;

            while ( link )
            {
                valueinternallink* linktorelease = link;
                link = link->next_;
                mapallocator ()->releasemaplink ( linktorelease );
            }
        }

        mapallocator ()->releasemapbuckets ( buckets_ );
    }
}


void
valueinternalmap::swap ( valueinternalmap& other )
{
    valueinternallink* tempbuckets = buckets_;
    buckets_ = other.buckets_;
    other.buckets_ = tempbuckets;
    valueinternallink* temptaillink = taillink_;
    taillink_ = other.taillink_;
    other.taillink_ = temptaillink;
    bucketindex tempbucketssize = bucketssize_;
    bucketssize_ = other.bucketssize_;
    other.bucketssize_ = tempbucketssize;
    bucketindex tempitemcount = itemcount_;
    itemcount_ = other.itemcount_;
    other.itemcount_ = tempitemcount;
}


void
valueinternalmap::clear ()
{
    valueinternalmap dummy;
    swap ( dummy );
}


valueinternalmap::bucketindex
valueinternalmap::size () const
{
    return itemcount_;
}

bool
valueinternalmap::reservedelta ( bucketindex growth )
{
    return reserve ( itemcount_ + growth );
}

bool
valueinternalmap::reserve ( bucketindex newitemcount )
{
    if ( !buckets_  &&  newitemcount > 0 )
    {
        buckets_ = mapallocator ()->allocatemapbuckets ( 1 );
        bucketssize_ = 1;
        taillink_ = &buckets_[0];
    }

    //   bucketindex idealbucketcount = (newitemcount + valueinternallink::itemperlink) / valueinternallink::itemperlink;
    return true;
}


const value*
valueinternalmap::find ( const char* key ) const
{
    if ( !bucketssize_ )
        return 0;

    hashkey hashedkey = hash ( key );
    bucketindex bucketindex = hashedkey % bucketssize_;

    for ( const valueinternallink* current = &buckets_[bucketindex];
            current != 0;
            current = current->next_ )
    {
        for ( bucketindex index = 0; index < valueinternallink::itemperlink; ++index )
        {
            if ( current->items_[index].isitemavailable () )
                return 0;

            if ( strcmp ( key, current->keys_[index] ) == 0 )
                return &current->items_[index];
        }
    }

    return 0;
}


value*
valueinternalmap::find ( const char* key )
{
    const valueinternalmap* constthis = this;
    return const_cast<value*> ( constthis->find ( key ) );
}


value&
valueinternalmap::resolvereference ( const char* key,
                                     bool isstatic )
{
    hashkey hashedkey = hash ( key );

    if ( bucketssize_ )
    {
        bucketindex bucketindex = hashedkey % bucketssize_;
        valueinternallink** previous = 0;
        bucketindex index;

        for ( valueinternallink* current = &buckets_[bucketindex];
                current != 0;
                previous = &current->next_, current = current->next_ )
        {
            for ( index = 0; index < valueinternallink::itemperlink; ++index )
            {
                if ( current->items_[index].isitemavailable () )
                    return setnewitem ( key, isstatic, current, index );

                if ( strcmp ( key, current->keys_[index] ) == 0 )
                    return current->items_[index];
            }
        }
    }

    reservedelta ( 1 );
    return unsafeadd ( key, isstatic, hashedkey );
}


void
valueinternalmap::remove ( const char* key )
{
    hashkey hashedkey = hash ( key );

    if ( !bucketssize_ )
        return;

    bucketindex bucketindex = hashedkey % bucketssize_;

    for ( valueinternallink* link = &buckets_[bucketindex];
            link != 0;
            link = link->next_ )
    {
        bucketindex index;

        for ( index = 0; index < valueinternallink::itemperlink; ++index )
        {
            if ( link->items_[index].isitemavailable () )
                return;

            if ( strcmp ( key, link->keys_[index] ) == 0 )
            {
                doactualremove ( link, index, bucketindex );
                return;
            }
        }
    }
}

void
valueinternalmap::doactualremove ( valueinternallink* link,
                                   bucketindex index,
                                   bucketindex bucketindex )
{
    // find last item of the bucket and swap it with the 'removed' one.
    // set removed items flags to 'available'.
    // if last page only contains 'available' items, then desallocate it (it's empty)
    valueinternallink*& lastlink = getlastlinkinbucket ( index );
    bucketindex lastitemindex = 1; // a link can never be empty, so start at 1

    for ( ;
            lastitemindex < valueinternallink::itemperlink;
            ++lastitemindex ) // may be optimized with dicotomic search
    {
        if ( lastlink->items_[lastitemindex].isitemavailable () )
            break;
    }

    bucketindex lastusedindex = lastitemindex - 1;
    value* valuetodelete = &link->items_[index];
    value* valuetopreserve = &lastlink->items_[lastusedindex];

    if ( valuetodelete != valuetopreserve )
        valuetodelete->swap ( *valuetopreserve );

    if ( lastusedindex == 0 )  // page is now empty
    {
        // remove it from bucket linked list and delete it.
        valueinternallink* linkprevioustolast = lastlink->previous_;

        if ( linkprevioustolast != 0 )   // can not deleted bucket link.
        {
            mapallocator ()->releasemaplink ( lastlink );
            linkprevioustolast->next_ = 0;
            lastlink = linkprevioustolast;
        }
    }
    else
    {
        value dummy;
        valuetopreserve->swap ( dummy ); // restore deleted to default value.
        valuetopreserve->setitemused ( false );
    }

    --itemcount_;
}


valueinternallink*&
valueinternalmap::getlastlinkinbucket ( bucketindex bucketindex )
{
    if ( bucketindex == bucketssize_ - 1 )
        return taillink_;

    valueinternallink*& previous = buckets_[bucketindex + 1].previous_;

    if ( !previous )
        previous = &buckets_[bucketindex];

    return previous;
}


value&
valueinternalmap::setnewitem ( const char* key,
                               bool isstatic,
                               valueinternallink* link,
                               bucketindex index )
{
    char* duplicatedkey = valueallocator ()->makemembername ( key );
    ++itemcount_;
    link->keys_[index] = duplicatedkey;
    link->items_[index].setitemused ();
    link->items_[index].setmembernameisstatic ( isstatic );
    return link->items_[index]; // items already default constructed.
}


value&
valueinternalmap::unsafeadd ( const char* key,
                              bool isstatic,
                              hashkey hashedkey )
{
    json_assert_message ( bucketssize_ > 0, "valueinternalmap::unsafeadd(): internal logic error." );
    bucketindex bucketindex = hashedkey % bucketssize_;
    valueinternallink*& previouslink = getlastlinkinbucket ( bucketindex );
    valueinternallink* link = previouslink;
    bucketindex index;

    for ( index = 0; index < valueinternallink::itemperlink; ++index )
    {
        if ( link->items_[index].isitemavailable () )
            break;
    }

    if ( index == valueinternallink::itemperlink ) // need to add a new page
    {
        valueinternallink* newlink = mapallocator ()->allocatemaplink ();
        index = 0;
        link->next_ = newlink;
        previouslink = newlink;
        link = newlink;
    }

    return setnewitem ( key, isstatic, link, index );
}


valueinternalmap::hashkey
valueinternalmap::hash ( const char* key ) const
{
    hashkey hash = 0;

    while ( *key )
        hash += *key++ * 37;

    return hash;
}


int
valueinternalmap::compare ( const valueinternalmap& other ) const
{
    int sizediff ( itemcount_ - other.itemcount_ );

    if ( sizediff != 0 )
        return sizediff;

    // strict order guaranty is required. compare all keys first, then compare values.
    iteratorstate it;
    iteratorstate itend;
    makebeginiterator ( it );
    makeenditerator ( itend );

    for ( ; !equals (it, itend); increment (it) )
    {
        if ( !other.find ( key ( it ) ) )
            return 1;
    }

    // all keys are equals, let's compare values
    makebeginiterator ( it );

    for ( ; !equals (it, itend); increment (it) )
    {
        const value* othervalue = other.find ( key ( it ) );
        int valuediff = value (it).compare ( *othervalue );

        if ( valuediff != 0 )
            return valuediff;
    }

    return 0;
}


void
valueinternalmap::makebeginiterator ( iteratorstate& it ) const
{
    it.map_ = const_cast<valueinternalmap*> ( this );
    it.bucketindex_ = 0;
    it.itemindex_ = 0;
    it.link_ = buckets_;
}


void
valueinternalmap::makeenditerator ( iteratorstate& it ) const
{
    it.map_ = const_cast<valueinternalmap*> ( this );
    it.bucketindex_ = bucketssize_;
    it.itemindex_ = 0;
    it.link_ = 0;
}


bool
valueinternalmap::equals ( const iteratorstate& x, const iteratorstate& other )
{
    return x.map_ == other.map_
           &&  x.bucketindex_ == other.bucketindex_
           &&  x.link_ == other.link_
           &&  x.itemindex_ == other.itemindex_;
}


void
valueinternalmap::incrementbucket ( iteratorstate& iterator )
{
    ++iterator.bucketindex_;
    json_assert_message ( iterator.bucketindex_ <= iterator.map_->bucketssize_,
                          "valueinternalmap::increment(): attempting to iterate beyond end." );

    if ( iterator.bucketindex_ == iterator.map_->bucketssize_ )
        iterator.link_ = 0;
    else
        iterator.link_ = & (iterator.map_->buckets_[iterator.bucketindex_]);

    iterator.itemindex_ = 0;
}


void
valueinternalmap::increment ( iteratorstate& iterator )
{
    json_assert_message ( iterator.map_, "attempting to iterator using invalid iterator." );
    ++iterator.itemindex_;

    if ( iterator.itemindex_ == valueinternallink::itemperlink )
    {
        json_assert_message ( iterator.link_ != 0,
                              "valueinternalmap::increment(): attempting to iterate beyond end." );
        iterator.link_ = iterator.link_->next_;

        if ( iterator.link_ == 0 )
            incrementbucket ( iterator );
    }
    else if ( iterator.link_->items_[iterator.itemindex_].isitemavailable () )
    {
        incrementbucket ( iterator );
    }
}


void
valueinternalmap::decrement ( iteratorstate& iterator )
{
    if ( iterator.itemindex_ == 0 )
    {
        json_assert_message ( iterator.map_, "attempting to iterate using invalid iterator." );

        if ( iterator.link_ == &iterator.map_->buckets_[iterator.bucketindex_] )
        {
            json_assert_message ( iterator.bucketindex_ > 0, "attempting to iterate beyond beginning." );
            -- (iterator.bucketindex_);
        }

        iterator.link_ = iterator.link_->previous_;
        iterator.itemindex_ = valueinternallink::itemperlink - 1;
    }
}


const char*
valueinternalmap::key ( const iteratorstate& iterator )
{
    json_assert_message ( iterator.link_, "attempting to iterate using invalid iterator." );
    return iterator.link_->keys_[iterator.itemindex_];
}

const char*
valueinternalmap::key ( const iteratorstate& iterator, bool& isstatic )
{
    json_assert_message ( iterator.link_, "attempting to iterate using invalid iterator." );
    isstatic = iterator.link_->items_[iterator.itemindex_].ismembernamestatic ();
    return iterator.link_->keys_[iterator.itemindex_];
}


value&
valueinternalmap::value ( const iteratorstate& iterator )
{
    json_assert_message ( iterator.link_, "attempting to iterate using invalid iterator." );
    return iterator.link_->items_[iterator.itemindex_];
}


int
valueinternalmap::distance ( const iteratorstate& x, const iteratorstate& y )
{
    int offset = 0;
    iteratorstate it = x;

    while ( !equals ( it, y ) )
        increment ( it );

    return offset;
}
