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
// class valueiteratorbase
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

valueiteratorbase::valueiteratorbase ()
#ifndef json_value_use_internal_map
    : current_ ()
    , isnull_ ( true )
{
}
#else
    :
    isarray_ ( true )
    , isnull_ ( true )
{
    iterator_.array_ = valueinternalarray::iteratorstate ();
}
#endif


#ifndef json_value_use_internal_map
valueiteratorbase::valueiteratorbase ( const value::objectvalues::iterator& current )
    : current_ ( current )
    , isnull_ ( false )
{
}
#else
valueiteratorbase::valueiteratorbase ( const valueinternalarray::iteratorstate& state )
    : isarray_ ( true )
{
    iterator_.array_ = state;
}


valueiteratorbase::valueiteratorbase ( const valueinternalmap::iteratorstate& state )
    : isarray_ ( false )
{
    iterator_.map_ = state;
}
#endif

value&
valueiteratorbase::deref () const
{
#ifndef json_value_use_internal_map
    return current_->second;
#else

    if ( isarray_ )
        return valueinternalarray::dereference ( iterator_.array_ );

    return valueinternalmap::value ( iterator_.map_ );
#endif
}


void
valueiteratorbase::increment ()
{
#ifndef json_value_use_internal_map
    ++current_;
#else

    if ( isarray_ )
        valueinternalarray::increment ( iterator_.array_ );

    valueinternalmap::increment ( iterator_.map_ );
#endif
}


void
valueiteratorbase::decrement ()
{
#ifndef json_value_use_internal_map
    --current_;
#else

    if ( isarray_ )
        valueinternalarray::decrement ( iterator_.array_ );

    valueinternalmap::decrement ( iterator_.map_ );
#endif
}


valueiteratorbase::difference_type
valueiteratorbase::computedistance ( const selftype& other ) const
{
#ifndef json_value_use_internal_map
# ifdef json_use_cpptl_smallmap
    return current_ - other.current_;
# else

    // iterator for null value are initialized using the default
    // constructor, which initialize current_ to the default
    // std::map::iterator. as begin() and end() are two instance
    // of the default std::map::iterator, they can not be compared.
    // to allow this, we handle this comparison specifically.
    if ( isnull_  &&  other.isnull_ )
    {
        return 0;
    }


    // usage of std::distance is not portable (does not compile with sun studio 12 roguewave stl,
    // which is the one used by default).
    // using a portable hand-made version for non random iterator instead:
    //   return difference_type( std::distance( current_, other.current_ ) );
    difference_type mydistance = 0;

    for ( value::objectvalues::iterator it = current_; it != other.current_; ++it )
    {
        ++mydistance;
    }

    return mydistance;
# endif
#else

    if ( isarray_ )
        return valueinternalarray::distance ( iterator_.array_, other.iterator_.array_ );

    return valueinternalmap::distance ( iterator_.map_, other.iterator_.map_ );
#endif
}


bool
valueiteratorbase::isequal ( const selftype& other ) const
{
#ifndef json_value_use_internal_map

    if ( isnull_ )
    {
        return other.isnull_;
    }

    return current_ == other.current_;
#else

    if ( isarray_ )
        return valueinternalarray::equals ( iterator_.array_, other.iterator_.array_ );

    return valueinternalmap::equals ( iterator_.map_, other.iterator_.map_ );
#endif
}


void
valueiteratorbase::copy ( const selftype& other )
{
#ifndef json_value_use_internal_map
    current_ = other.current_;
#else

    if ( isarray_ )
        iterator_.array_ = other.iterator_.array_;

    iterator_.map_ = other.iterator_.map_;
#endif
}


value
valueiteratorbase::key () const
{
#ifndef json_value_use_internal_map
    const value::czstring czstring = (*current_).first;

    if ( czstring.c_str () )
    {
        if ( czstring.isstaticstring () )
            return value ( staticstring ( czstring.c_str () ) );

        return value ( czstring.c_str () );
    }

    return value ( czstring.index () );
#else

    if ( isarray_ )
        return value ( valueinternalarray::indexof ( iterator_.array_ ) );

    bool isstatic;
    const char* membername = valueinternalmap::key ( iterator_.map_, isstatic );

    if ( isstatic )
        return value ( staticstring ( membername ) );

    return value ( membername );
#endif
}


uint
valueiteratorbase::index () const
{
#ifndef json_value_use_internal_map
    const value::czstring czstring = (*current_).first;

    if ( !czstring.c_str () )
        return czstring.index ();

    return value::uint ( -1 );
#else

    if ( isarray_ )
        return value::uint ( valueinternalarray::indexof ( iterator_.array_ ) );

    return value::uint ( -1 );
#endif
}


const char*
valueiteratorbase::membername () const
{
#ifndef json_value_use_internal_map
    const char* name = (*current_).first.c_str ();
    return name ? name : "";
#else

    if ( !isarray_ )
        return valueinternalmap::key ( iterator_.map_ );

    return "";
#endif
}


// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class valueconstiterator
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

valueconstiterator::valueconstiterator ()
{
}


#ifndef json_value_use_internal_map
valueconstiterator::valueconstiterator ( const value::objectvalues::iterator& current )
    : valueiteratorbase ( current )
{
}
#else
valueconstiterator::valueconstiterator ( const valueinternalarray::iteratorstate& state )
    : valueiteratorbase ( state )
{
}

valueconstiterator::valueconstiterator ( const valueinternalmap::iteratorstate& state )
    : valueiteratorbase ( state )
{
}
#endif

valueconstiterator&
valueconstiterator::operator = ( const valueiteratorbase& other )
{
    copy ( other );
    return *this;
}


// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class valueiterator
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

valueiterator::valueiterator ()
{
}


#ifndef json_value_use_internal_map
valueiterator::valueiterator ( const value::objectvalues::iterator& current )
    : valueiteratorbase ( current )
{
}
#else
valueiterator::valueiterator ( const valueinternalarray::iteratorstate& state )
    : valueiteratorbase ( state )
{
}

valueiterator::valueiterator ( const valueinternalmap::iteratorstate& state )
    : valueiteratorbase ( state )
{
}
#endif

valueiterator::valueiterator ( const valueconstiterator& other )
    : valueiteratorbase ( other )
{
}

valueiterator::valueiterator ( const valueiterator& other )
    : valueiteratorbase ( other )
{
}

valueiterator&
valueiterator::operator = ( const selftype& other )
{
    copy ( other );
    return *this;
}
