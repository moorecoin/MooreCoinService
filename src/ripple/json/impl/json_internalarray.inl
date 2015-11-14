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
// class valueinternalarray
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

valuearrayallocator::~valuearrayallocator ()
{
}

// //////////////////////////////////////////////////////////////////
// class defaultvaluearrayallocator
// //////////////////////////////////////////////////////////////////
#ifdef json_use_simple_internal_allocator
class defaultvaluearrayallocator : public valuearrayallocator
{
public: // overridden from valuearrayallocator
    virtual ~defaultvaluearrayallocator ()
    {
    }

    virtual valueinternalarray* newarray ()
    {
        return new valueinternalarray ();
    }

    virtual valueinternalarray* newarraycopy ( const valueinternalarray& other )
    {
        return new valueinternalarray ( other );
    }

    virtual void destructarray ( valueinternalarray* array )
    {
        delete array;
    }

    virtual void reallocatearraypageindex ( value**& indexes,
                                            valueinternalarray::pageindex& indexcount,
                                            valueinternalarray::pageindex minnewindexcount )
    {
        valueinternalarray::pageindex newindexcount = (indexcount * 3) / 2 + 1;

        if ( minnewindexcount > newindexcount )
            newindexcount = minnewindexcount;

        void* newindexes = realloc ( indexes, sizeof (value*) * newindexcount );

        if ( !newindexes )
            throw std::bad_alloc ();

        indexcount = newindexcount;
        indexes = static_cast<value**> ( newindexes );
    }
    virtual void releasearraypageindex ( value** indexes,
                                         valueinternalarray::pageindex indexcount )
    {
        if ( indexes )
            free ( indexes );
    }

    virtual value* allocatearraypage ()
    {
        return static_cast<value*> ( malloc ( sizeof (value) * valueinternalarray::itemsperpage ) );
    }

    virtual void releasearraypage ( value* value )
    {
        if ( value )
            free ( value );
    }
};

#else // #ifdef json_use_simple_internal_allocator
/// @todo make this thread-safe (lock when accessign batch allocator)
class defaultvaluearrayallocator : public valuearrayallocator
{
public: // overridden from valuearrayallocator
    virtual ~defaultvaluearrayallocator ()
    {
    }

    virtual valueinternalarray* newarray ()
    {
        valueinternalarray* array = arraysallocator_.allocate ();
        new (array) valueinternalarray (); // placement new
        return array;
    }

    virtual valueinternalarray* newarraycopy ( const valueinternalarray& other )
    {
        valueinternalarray* array = arraysallocator_.allocate ();
        new (array) valueinternalarray ( other ); // placement new
        return array;
    }

    virtual void destructarray ( valueinternalarray* array )
    {
        if ( array )
        {
            array->~valueinternalarray ();
            arraysallocator_.release ( array );
        }
    }

    virtual void reallocatearraypageindex ( value**& indexes,
                                            valueinternalarray::pageindex& indexcount,
                                            valueinternalarray::pageindex minnewindexcount )
    {
        valueinternalarray::pageindex newindexcount = (indexcount * 3) / 2 + 1;

        if ( minnewindexcount > newindexcount )
            newindexcount = minnewindexcount;

        void* newindexes = realloc ( indexes, sizeof (value*) * newindexcount );

        if ( !newindexes )
            throw std::bad_alloc ();

        indexcount = newindexcount;
        indexes = static_cast<value**> ( newindexes );
    }
    virtual void releasearraypageindex ( value** indexes,
                                         valueinternalarray::pageindex indexcount )
    {
        if ( indexes )
            free ( indexes );
    }

    virtual value* allocatearraypage ()
    {
        return static_cast<value*> ( pagesallocator_.allocate () );
    }

    virtual void releasearraypage ( value* value )
    {
        if ( value )
            pagesallocator_.release ( value );
    }
private:
    batchallocator<valueinternalarray, 1> arraysallocator_;
    batchallocator<value, valueinternalarray::itemsperpage> pagesallocator_;
};
#endif // #ifdef json_use_simple_internal_allocator

static valuearrayallocator*& arrayallocator ()
{
    static defaultvaluearrayallocator defaultallocator;
    static valuearrayallocator* arrayallocator = &defaultallocator;
    return arrayallocator;
}

static struct dummyarrayallocatorinitializer
{
    dummyarrayallocatorinitializer ()
    {
        arrayallocator ();     // ensure arrayallocator() statics are initialized before main().
    }
} dummyarrayallocatorinitializer;

// //////////////////////////////////////////////////////////////////
// class valueinternalarray
// //////////////////////////////////////////////////////////////////
bool
valueinternalarray::equals ( const iteratorstate& x,
                             const iteratorstate& other )
{
    return x.array_ == other.array_
           &&  x.currentitemindex_ == other.currentitemindex_
           &&  x.currentpageindex_ == other.currentpageindex_;
}


void
valueinternalarray::increment ( iteratorstate& it )
{
    json_assert_message ( it.array_  &&
                          (it.currentpageindex_ - it.array_->pages_)*itemsperpage + it.currentitemindex_
                          != it.array_->size_,
                          "valueinternalarray::increment(): moving iterator beyond end" );
    ++ (it.currentitemindex_);

    if ( it.currentitemindex_ == itemsperpage )
    {
        it.currentitemindex_ = 0;
        ++ (it.currentpageindex_);
    }
}


void
valueinternalarray::decrement ( iteratorstate& it )
{
    json_assert_message ( it.array_  &&  it.currentpageindex_ == it.array_->pages_
                          &&  it.currentitemindex_ == 0,
                          "valueinternalarray::decrement(): moving iterator beyond end" );

    if ( it.currentitemindex_ == 0 )
    {
        it.currentitemindex_ = itemsperpage - 1;
        -- (it.currentpageindex_);
    }
    else
    {
        -- (it.currentitemindex_);
    }
}


value&
valueinternalarray::unsafedereference ( const iteratorstate& it )
{
    return (* (it.currentpageindex_))[it.currentitemindex_];
}


value&
valueinternalarray::dereference ( const iteratorstate& it )
{
    json_assert_message ( it.array_  &&
                          (it.currentpageindex_ - it.array_->pages_)*itemsperpage + it.currentitemindex_
                          < it.array_->size_,
                          "valueinternalarray::dereference(): dereferencing invalid iterator" );
    return unsafedereference ( it );
}

void
valueinternalarray::makebeginiterator ( iteratorstate& it ) const
{
    it.array_ = const_cast<valueinternalarray*> ( this );
    it.currentitemindex_ = 0;
    it.currentpageindex_ = pages_;
}


void
valueinternalarray::makeiterator ( iteratorstate& it, arrayindex index ) const
{
    it.array_ = const_cast<valueinternalarray*> ( this );
    it.currentitemindex_ = index % itemsperpage;
    it.currentpageindex_ = pages_ + index / itemsperpage;
}


void
valueinternalarray::makeenditerator ( iteratorstate& it ) const
{
    makeiterator ( it, size_ );
}


valueinternalarray::valueinternalarray ()
    : pages_ ( 0 )
    , size_ ( 0 )
    , pagecount_ ( 0 )
{
}


valueinternalarray::valueinternalarray ( const valueinternalarray& other )
    : pages_ ( 0 )
    , pagecount_ ( 0 )
    , size_ ( other.size_ )
{
    pageindex minnewpages = other.size_ / itemsperpage;
    arrayallocator ()->reallocatearraypageindex ( pages_, pagecount_, minnewpages );
    json_assert_message ( pagecount_ >= minnewpages,
                          "valueinternalarray::reserve(): bad reallocation" );
    iteratorstate itother;
    other.makebeginiterator ( itother );
    value* value;

    for ( arrayindex index = 0; index < size_; ++index, increment (itother) )
    {
        if ( index % itemsperpage == 0 )
        {
            pageindex pageindex = index / itemsperpage;
            value = arrayallocator ()->allocatearraypage ();
            pages_[pageindex] = value;
        }

        new (value) value ( dereference ( itother ) );
    }
}


valueinternalarray&
valueinternalarray::operator = ( const valueinternalarray& other )
{
    valueinternalarray temp ( other );
    swap ( temp );
    return *this;
}


valueinternalarray::~valueinternalarray ()
{
    // destroy all constructed items
    iteratorstate it;
    iteratorstate itend;
    makebeginiterator ( it);
    makeenditerator ( itend );

    for ( ; !equals (it, itend); increment (it) )
    {
        value* value = &dereference (it);
        value->~value ();
    }

    // release all pages
    pageindex lastpageindex = size_ / itemsperpage;

    for ( pageindex pageindex = 0; pageindex < lastpageindex; ++pageindex )
        arrayallocator ()->releasearraypage ( pages_[pageindex] );

    // release pages index
    arrayallocator ()->releasearraypageindex ( pages_, pagecount_ );
}


void
valueinternalarray::swap ( valueinternalarray& other )
{
    value** temppages = pages_;
    pages_ = other.pages_;
    other.pages_ = temppages;
    arrayindex tempsize = size_;
    size_ = other.size_;
    other.size_ = tempsize;
    pageindex temppagecount = pagecount_;
    pagecount_ = other.pagecount_;
    other.pagecount_ = temppagecount;
}

void
valueinternalarray::clear ()
{
    valueinternalarray dummy;
    swap ( dummy );
}


void
valueinternalarray::resize ( arrayindex newsize )
{
    if ( newsize == 0 )
        clear ();
    else if ( newsize < size_ )
    {
        iteratorstate it;
        iteratorstate itend;
        makeiterator ( it, newsize );
        makeiterator ( itend, size_ );

        for ( ; !equals (it, itend); increment (it) )
        {
            value* value = &dereference (it);
            value->~value ();
        }

        pageindex pageindex = (newsize + itemsperpage - 1) / itemsperpage;
        pageindex lastpageindex = size_ / itemsperpage;

        for ( ; pageindex < lastpageindex; ++pageindex )
            arrayallocator ()->releasearraypage ( pages_[pageindex] );

        size_ = newsize;
    }
    else if ( newsize > size_ )
        resolvereference ( newsize );
}


void
valueinternalarray::makeindexvalid ( arrayindex index )
{
    // need to enlarge page index ?
    if ( index >= pagecount_ * itemsperpage )
    {
        pageindex minnewpages = (index + 1) / itemsperpage;
        arrayallocator ()->reallocatearraypageindex ( pages_, pagecount_, minnewpages );
        json_assert_message ( pagecount_ >= minnewpages, "valueinternalarray::reserve(): bad reallocation" );
    }

    // need to allocate new pages ?
    arrayindex nextpageindex =
        (size_ % itemsperpage) != 0 ? size_ - (size_ % itemsperpage) + itemsperpage
        : size_;

    if ( nextpageindex <= index )
    {
        pageindex pageindex = nextpageindex / itemsperpage;
        pageindex pagetoallocate = (index - nextpageindex) / itemsperpage + 1;

        for ( ; pagetoallocate-- > 0; ++pageindex )
            pages_[pageindex] = arrayallocator ()->allocatearraypage ();
    }

    // initialize all new entries
    iteratorstate it;
    iteratorstate itend;
    makeiterator ( it, size_ );
    size_ = index + 1;
    makeiterator ( itend, size_ );

    for ( ; !equals (it, itend); increment (it) )
    {
        value* value = &dereference (it);
        new (value) value (); // construct a default value using placement new
    }
}

value&
valueinternalarray::resolvereference ( arrayindex index )
{
    if ( index >= size_ )
        makeindexvalid ( index );

    return pages_[index / itemsperpage][index % itemsperpage];
}

value*
valueinternalarray::find ( arrayindex index ) const
{
    if ( index >= size_ )
        return 0;

    return & (pages_[index / itemsperpage][index % itemsperpage]);
}

valueinternalarray::arrayindex
valueinternalarray::size () const
{
    return size_;
}

int
valueinternalarray::distance ( const iteratorstate& x, const iteratorstate& y )
{
    return indexof (y) - indexof (x);
}


valueinternalarray::arrayindex
valueinternalarray::indexof ( const iteratorstate& iterator )
{
    if ( !iterator.array_ )
        return arrayindex (-1);

    return arrayindex (
               (iterator.currentpageindex_ - iterator.array_->pages_) * itemsperpage
               + iterator.currentitemindex_ );
}


int
valueinternalarray::compare ( const valueinternalarray& other ) const
{
    int sizediff ( size_ - other.size_ );

    if ( sizediff != 0 )
        return sizediff;

    for ( arrayindex index = 0; index < size_; ++index )
    {
        int diff = pages_[index / itemsperpage][index % itemsperpage].compare (
                       other.pages_[index / itemsperpage][index % itemsperpage] );

        if ( diff != 0 )
            return diff;
    }

    return 0;
}
