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

#include <beastconfig.h>
#include <ripple/json/impl/json_assert.h>
#include <ripple/json/to_string.h>
#include <ripple/json/json_writer.h>
#include <beast/module/core/text/lexicalcast.h>

namespace json {

const value value::null;
const int value::minint = int ( ~ (uint (-1) / 2) );
const int value::maxint = int ( uint (-1) / 2 );
const uint value::maxuint = uint (-1);

// a "safe" implementation of strdup. allow null pointer to be passed.
// also avoid warning on msvc80.
//
//inline char *safestringdup( const char *czstring )
//{
//   if ( czstring )
//   {
//      const size_t length = (unsigned int)( strlen(czstring) + 1 );
//      char *newstring = static_cast<char *>( malloc( length ) );
//      memcpy( newstring, czstring, length );
//      return newstring;
//   }
//   return 0;
//}
//
//inline char *safestringdup( std::string const&str )
//{
//   if ( !str.empty() )
//   {
//      const size_t length = str.length();
//      char *newstring = static_cast<char *>( malloc( length + 1 ) );
//      memcpy( newstring, str.c_str(), length );
//      newstring[length] = 0;
//      return newstring;
//   }
//   return 0;
//}

valueallocator::~valueallocator ()
{
}

class defaultvalueallocator : public valueallocator
{
public:
    virtual ~defaultvalueallocator ()
    {
    }

    virtual char* makemembername ( const char* membername )
    {
        return duplicatestringvalue ( membername );
    }

    virtual void releasemembername ( char* membername )
    {
        releasestringvalue ( membername );
    }

    virtual char* duplicatestringvalue ( const char* value,
                                         unsigned int length = unknown )
    {
        //@todo invesgate this old optimization
        //if ( !value  ||  value[0] == 0 )
        //   return 0;

        if ( length == unknown )
            length = (unsigned int)strlen (value);

        char* newstring = static_cast<char*> ( malloc ( length + 1 ) );
        memcpy ( newstring, value, length );
        newstring[length] = 0;
        return newstring;
    }

    virtual void releasestringvalue ( char* value )
    {
        if ( value )
            free ( value );
    }
};

static valueallocator*& valueallocator ()
{
    static valueallocator* valueallocator = new defaultvalueallocator;
    return valueallocator;
}

static struct dummyvalueallocatorinitializer
{
    dummyvalueallocatorinitializer ()
    {
        valueallocator ();     // ensure valueallocator() statics are initialized before main().
    }
} dummyvalueallocatorinitializer;



// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// valueinternals...
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
#ifdef json_value_use_internal_map
# include <ripple/json/impl/json_internalarray.inl>
# include <ripple/json/impl/json_internalmap.inl>
#endif // json_value_use_internal_map

# include <ripple/json/impl/json_valueiterator.inl>


// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class value::commentinfo
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////


value::commentinfo::commentinfo ()
    : comment_ ( 0 )
{
}

value::commentinfo::~commentinfo ()
{
    if ( comment_ )
        valueallocator ()->releasestringvalue ( comment_ );
}


void
value::commentinfo::setcomment ( const char* text )
{
    if ( comment_ )
        valueallocator ()->releasestringvalue ( comment_ );

    json_assert ( text );
    json_assert_message ( text[0] == '\0' || text[0] == '/', "comments must start with /");
    // it seems that /**/ style comments are acceptable as well.
    comment_ = valueallocator ()->duplicatestringvalue ( text );
}


// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class value::czstring
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
# ifndef json_value_use_internal_map

// notes: index_ indicates if the string was allocated when
// a string is stored.

value::czstring::czstring ( int index )
    : cstr_ ( 0 )
    , index_ ( index )
{
}

value::czstring::czstring ( const char* cstr, duplicationpolicy allocate )
    : cstr_ ( allocate == duplicate ? valueallocator ()->makemembername (cstr)
              : cstr )
    , index_ ( allocate )
{
}

value::czstring::czstring ( const czstring& other )
    : cstr_ ( other.index_ != noduplication&&   other.cstr_ != 0
              ?  valueallocator ()->makemembername ( other.cstr_ )
              : other.cstr_ )
    , index_ ( other.cstr_ ? (other.index_ == noduplication ? noduplication : duplicate)
               : other.index_ )
{
}

value::czstring::~czstring ()
{
    if ( cstr_  &&  index_ == duplicate )
        valueallocator ()->releasemembername ( const_cast<char*> ( cstr_ ) );
}

void
value::czstring::swap ( czstring& other )
{
    std::swap ( cstr_, other.cstr_ );
    std::swap ( index_, other.index_ );
}

value::czstring&
value::czstring::operator = ( const czstring& other )
{
    czstring temp ( other );
    swap ( temp );
    return *this;
}

bool
value::czstring::operator< ( const czstring& other ) const
{
    if ( cstr_ )
        return strcmp ( cstr_, other.cstr_ ) < 0;

    return index_ < other.index_;
}

bool
value::czstring::operator== ( const czstring& other ) const
{
    if ( cstr_ )
        return strcmp ( cstr_, other.cstr_ ) == 0;

    return index_ == other.index_;
}


int
value::czstring::index () const
{
    return index_;
}


const char*
value::czstring::c_str () const
{
    return cstr_;
}

bool
value::czstring::isstaticstring () const
{
    return index_ == noduplication;
}

#endif // ifndef json_value_use_internal_map


// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class value::value
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

/*! \internal default constructor initialization must be equivalent to:
 * memset( this, 0, sizeof(value) )
 * this optimization is used in valueinternalmap fast allocator.
 */
value::value ( valuetype type )
    : type_ ( type )
    , allocated_ ( 0 )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    switch ( type )
    {
    case nullvalue:
        break;

    case intvalue:
    case uintvalue:
        value_.int_ = 0;
        break;

    case realvalue:
        value_.real_ = 0.0;
        break;

    case stringvalue:
        value_.string_ = 0;
        break;
#ifndef json_value_use_internal_map

    case arrayvalue:
    case objectvalue:
        value_.map_ = new objectvalues ();
        break;
#else

    case arrayvalue:
        value_.array_ = arrayallocator ()->newarray ();
        break;

    case objectvalue:
        value_.map_ = mapallocator ()->newmap ();
        break;
#endif

    case booleanvalue:
        value_.bool_ = false;
        break;

    default:
        json_assert_unreachable;
    }
}


value::value ( int value )
    : type_ ( intvalue )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    value_.int_ = value;
}


value::value ( uint value )
    : type_ ( uintvalue )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    value_.uint_ = value;
}

value::value ( double value )
    : type_ ( realvalue )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    value_.real_ = value;
}

value::value ( const char* value )
    : type_ ( stringvalue )
    , allocated_ ( true )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    value_.string_ = valueallocator ()->duplicatestringvalue ( value );
}


value::value ( const char* beginvalue,
               const char* endvalue )
    : type_ ( stringvalue )
    , allocated_ ( true )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    value_.string_ = valueallocator ()->duplicatestringvalue ( beginvalue,
                     uint (endvalue - beginvalue) );
}


value::value ( std::string const& value )
    : type_ ( stringvalue )
    , allocated_ ( true )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    value_.string_ = valueallocator ()->duplicatestringvalue ( value.c_str (),
                     (unsigned int)value.length () );

}

value::value (beast::string const& beaststring)
    : type_ ( stringvalue )
    , allocated_ ( true )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    value_.string_ = valueallocator ()->duplicatestringvalue ( beaststring.tostdstring ().c_str (),
                     (unsigned int)beaststring.length () );

}

value::value ( const staticstring& value )
    : type_ ( stringvalue )
    , allocated_ ( false )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    value_.string_ = const_cast<char*> ( value.c_str () );
}


# ifdef json_use_cpptl
value::value ( const cpptl::conststring& value )
    : type_ ( stringvalue )
    , allocated_ ( true )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    value_.string_ = valueallocator ()->duplicatestringvalue ( value, value.length () );
}
# endif

value::value ( bool value )
    : type_ ( booleanvalue )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    value_.bool_ = value;
}


value::value ( const value& other )
    : type_ ( other.type_ )
    , comments_ ( 0 )
# ifdef json_value_use_internal_map
    , itemisused_ ( 0 )
#endif
{
    switch ( type_ )
    {
    case nullvalue:
    case intvalue:
    case uintvalue:
    case realvalue:
    case booleanvalue:
        value_ = other.value_;
        break;

    case stringvalue:
        if ( other.value_.string_ )
        {
            value_.string_ = valueallocator ()->duplicatestringvalue ( other.value_.string_ );
            allocated_ = true;
        }
        else
            value_.string_ = 0;

        break;
#ifndef json_value_use_internal_map

    case arrayvalue:
    case objectvalue:
        value_.map_ = new objectvalues ( *other.value_.map_ );
        break;
#else

    case arrayvalue:
        value_.array_ = arrayallocator ()->newarraycopy ( *other.value_.array_ );
        break;

    case objectvalue:
        value_.map_ = mapallocator ()->newmapcopy ( *other.value_.map_ );
        break;
#endif

    default:
        json_assert_unreachable;
    }

    if ( other.comments_ )
    {
        comments_ = new commentinfo[numberofcommentplacement];

        for ( int comment = 0; comment < numberofcommentplacement; ++comment )
        {
            const commentinfo& othercomment = other.comments_[comment];

            if ( othercomment.comment_ )
                comments_[comment].setcomment ( othercomment.comment_ );
        }
    }
}


value::~value ()
{
    switch ( type_ )
    {
    case nullvalue:
    case intvalue:
    case uintvalue:
    case realvalue:
    case booleanvalue:
        break;

    case stringvalue:
        if ( allocated_ )
            valueallocator ()->releasestringvalue ( value_.string_ );

        break;
#ifndef json_value_use_internal_map

    case arrayvalue:
    case objectvalue:
        delete value_.map_;
        break;
#else

    case arrayvalue:
        arrayallocator ()->destructarray ( value_.array_ );
        break;

    case objectvalue:
        mapallocator ()->destructmap ( value_.map_ );
        break;
#endif

    default:
        json_assert_unreachable;
    }

    if ( comments_ )
        delete[] comments_;
}

value&
value::operator= ( const value& other )
{
    value temp ( other );
    swap ( temp );
    return *this;
}

value::value ( value&& other )
    : value_ ( other.value_ )
    , type_ ( other.type_ )
    , allocated_ ( other.allocated_ )
# ifdef json_value_use_internal_map
    , itemisused_ ( other.itemisused_ )
    , membernameisstatic_ ( other.membernameisstatic_ )
#endif  // json_value_use_internal_map
    , comments_ ( other.comments_ )
{
    std::memset( &other, 0, sizeof(value) );
}

value&
value::operator= ( value&& other )
{
    swap ( other );
    return *this;
}

void
value::swap ( value& other )
{
    std::swap ( value_, other.value_ );

    valuetype temp = type_;
    type_ = other.type_;
    other.type_ = temp;

    int temp2 = allocated_;
    allocated_ = other.allocated_;
    other.allocated_ = temp2;

# ifdef json_value_use_internal_map
    unsigned temp3 = itemisused_;
    itemisused_ = other.itemisused_;
    other.itemisused_ = itemisused_;

    temp2 = membernameisstatic_;
    membernameisstatic_ = other.membernameisstatic_;
    other.membernameisstatic_ = temp2
# endif
    std::swap(comments_, other.comments_);
}

valuetype
value::type () const
{
    return type_;
}


int
value::compare ( const value& other )
{
    /*
    int typedelta = other.type_ - type_;
    switch ( type_ )
    {
    case nullvalue:

       return other.type_ == type_;
    case intvalue:
       if ( other.type_.isnumeric()
    case uintvalue:
    case realvalue:
    case booleanvalue:
       break;
    case stringvalue,
       break;
    case arrayvalue:
       delete value_.array_;
       break;
    case objectvalue:
       delete value_.map_;
    default:
       json_assert_unreachable;
    }
    */
    return 0;  // unreachable
}

bool
value::operator < ( const value& other ) const
{
    int typedelta = type_ - other.type_;

    if ( typedelta )
        return typedelta < 0 ? true : false;

    switch ( type_ )
    {
    case nullvalue:
        return false;

    case intvalue:
        return value_.int_ < other.value_.int_;

    case uintvalue:
        return value_.uint_ < other.value_.uint_;

    case realvalue:
        return value_.real_ < other.value_.real_;

    case booleanvalue:
        return value_.bool_ < other.value_.bool_;

    case stringvalue:
        return ( value_.string_ == 0  &&  other.value_.string_ )
               || ( other.value_.string_
                    &&  value_.string_
                    && strcmp ( value_.string_, other.value_.string_ ) < 0 );
#ifndef json_value_use_internal_map

    case arrayvalue:
    case objectvalue:
    {
        int delta = int ( value_.map_->size () - other.value_.map_->size () );

        if ( delta )
            return delta < 0;

        return (*value_.map_) < (*other.value_.map_);
    }

#else

    case arrayvalue:
        return value_.array_->compare ( * (other.value_.array_) ) < 0;

    case objectvalue:
        return value_.map_->compare ( * (other.value_.map_) ) < 0;
#endif

    default:
        json_assert_unreachable;
    }

    return 0;  // unreachable
}

bool
value::operator <= ( const value& other ) const
{
    return ! (other > *this);
}

bool
value::operator >= ( const value& other ) const
{
    return ! (*this < other);
}

bool
value::operator > ( const value& other ) const
{
    return other < *this;
}

bool
value::operator == ( const value& other ) const
{
    //if ( type_ != other.type_ )
    // gcc 2.95.3 says:
    // attempt to take address of bit-field structure member `json::value::type_'
    // beats me, but a temp solves the problem.
    int temp = other.type_;

    if ( type_ != temp )
        return false;

    switch ( type_ )
    {
    case nullvalue:
        return true;

    case intvalue:
        return value_.int_ == other.value_.int_;

    case uintvalue:
        return value_.uint_ == other.value_.uint_;

    case realvalue:
        return value_.real_ == other.value_.real_;

    case booleanvalue:
        return value_.bool_ == other.value_.bool_;

    case stringvalue:
        return ( value_.string_ == other.value_.string_ )
               || ( other.value_.string_
                    &&  value_.string_
                    && strcmp ( value_.string_, other.value_.string_ ) == 0 );
#ifndef json_value_use_internal_map

    case arrayvalue:
    case objectvalue:
        return value_.map_->size () == other.value_.map_->size ()
               && (*value_.map_) == (*other.value_.map_);
#else

    case arrayvalue:
        return value_.array_->compare ( * (other.value_.array_) ) == 0;

    case objectvalue:
        return value_.map_->compare ( * (other.value_.map_) ) == 0;
#endif

    default:
        json_assert_unreachable;
    }

    return 0;  // unreachable
}

bool
value::operator != ( const value& other ) const
{
    return ! ( *this == other );
}

const char*
value::ascstring () const
{
    json_assert ( type_ == stringvalue );
    return value_.string_;
}


std::string
value::asstring () const
{
    switch ( type_ )
    {
    case nullvalue:
        return "";

    case stringvalue:
        return value_.string_ ? value_.string_ : "";

    case booleanvalue:
        return value_.bool_ ? "true" : "false";

    case intvalue:
        return beast::lexicalcastthrow <std::string> (value_.int_);

    case uintvalue:
    case realvalue:
    case arrayvalue:
    case objectvalue:
        json_assert_message ( false, "type is not convertible to string" );

    default:
        json_assert_unreachable;
    }

    return ""; // unreachable
}

# ifdef json_use_cpptl
cpptl::conststring
value::asconststring () const
{
    return cpptl::conststring ( asstring ().c_str () );
}
# endif

value::int
value::asint () const
{
    switch ( type_ )
    {
    case nullvalue:
        return 0;

    case intvalue:
        return value_.int_;

    case uintvalue:
        json_assert_message ( value_.uint_ < (unsigned)maxint, "integer out of signed integer range" );
        return value_.uint_;

    case realvalue:
        json_assert_message ( value_.real_ >= minint  &&  value_.real_ <= maxint, "real out of signed integer range" );
        return int ( value_.real_ );

    case booleanvalue:
        return value_.bool_ ? 1 : 0;

    case stringvalue:
        return beast::lexicalcastthrow <int> (value_.string_);

    case arrayvalue:
    case objectvalue:
        json_assert_message ( false, "type is not convertible to int" );

    default:
        json_assert_unreachable;
    }

    return 0; // unreachable;
}

value::uint
value::asuint () const
{
    switch ( type_ )
    {
    case nullvalue:
        return 0;

    case intvalue:
        json_assert_message ( value_.int_ >= 0, "negative integer can not be converted to unsigned integer" );
        return value_.int_;

    case uintvalue:
        return value_.uint_;

    case realvalue:
        json_assert_message ( value_.real_ >= 0  &&  value_.real_ <= maxuint,  "real out of unsigned integer range" );
        return uint ( value_.real_ );

    case booleanvalue:
        return value_.bool_ ? 1 : 0;

    case stringvalue:
        return beast::lexicalcastthrow <unsigned int> (value_.string_);

    case arrayvalue:
    case objectvalue:
        json_assert_message ( false, "type is not convertible to uint" );

    default:
        json_assert_unreachable;
    }

    return 0; // unreachable;
}

double
value::asdouble () const
{
    switch ( type_ )
    {
    case nullvalue:
        return 0.0;

    case intvalue:
        return value_.int_;

    case uintvalue:
        return value_.uint_;

    case realvalue:
        return value_.real_;

    case booleanvalue:
        return value_.bool_ ? 1.0 : 0.0;

    case stringvalue:
    case arrayvalue:
    case objectvalue:
        json_assert_message ( false, "type is not convertible to double" );

    default:
        json_assert_unreachable;
    }

    return 0; // unreachable;
}

bool
value::asbool () const
{
    switch ( type_ )
    {
    case nullvalue:
        return false;

    case intvalue:
    case uintvalue:
        return value_.int_ != 0;

    case realvalue:
        return value_.real_ != 0.0;

    case booleanvalue:
        return value_.bool_;

    case stringvalue:
        return value_.string_  &&  value_.string_[0] != 0;

    case arrayvalue:
    case objectvalue:
        return value_.map_->size () != 0;

    default:
        json_assert_unreachable;
    }

    return false; // unreachable;
}


bool
value::isconvertibleto ( valuetype other ) const
{
    switch ( type_ )
    {
    case nullvalue:
        return true;

    case intvalue:
        return ( other == nullvalue  &&  value_.int_ == 0 )
               || other == intvalue
               || ( other == uintvalue  && value_.int_ >= 0 )
               || other == realvalue
               || other == stringvalue
               || other == booleanvalue;

    case uintvalue:
        return ( other == nullvalue  &&  value_.uint_ == 0 )
               || ( other == intvalue  && value_.uint_ <= (unsigned)maxint )
               || other == uintvalue
               || other == realvalue
               || other == stringvalue
               || other == booleanvalue;

    case realvalue:
        return ( other == nullvalue  &&  value_.real_ == 0.0 )
               || ( other == intvalue  &&  value_.real_ >= minint  &&  value_.real_ <= maxint )
               || ( other == uintvalue  &&  value_.real_ >= 0  &&  value_.real_ <= maxuint )
               || other == realvalue
               || other == stringvalue
               || other == booleanvalue;

    case booleanvalue:
        return ( other == nullvalue  &&  value_.bool_ == false )
               || other == intvalue
               || other == uintvalue
               || other == realvalue
               || other == stringvalue
               || other == booleanvalue;

    case stringvalue:
        return other == stringvalue
               || ( other == nullvalue  &&  (!value_.string_  ||  value_.string_[0] == 0) );

    case arrayvalue:
        return other == arrayvalue
               ||  ( other == nullvalue  &&  value_.map_->size () == 0 );

    case objectvalue:
        return other == objectvalue
               ||  ( other == nullvalue  &&  value_.map_->size () == 0 );

    default:
        json_assert_unreachable;
    }

    return false; // unreachable;
}


/// number of values in array or object
value::uint
value::size () const
{
    switch ( type_ )
    {
    case nullvalue:
    case intvalue:
    case uintvalue:
    case realvalue:
    case booleanvalue:
    case stringvalue:
        return 0;
#ifndef json_value_use_internal_map

    case arrayvalue:  // size of the array is highest index + 1
        if ( !value_.map_->empty () )
        {
            objectvalues::const_iterator itlast = value_.map_->end ();
            --itlast;
            return (*itlast).first.index () + 1;
        }

        return 0;

    case objectvalue:
        return int ( value_.map_->size () );
#else

    case arrayvalue:
        return int ( value_.array_->size () );

    case objectvalue:
        return int ( value_.map_->size () );
#endif

    default:
        json_assert_unreachable;
    }

    return 0; // unreachable;
}


bool
value::empty () const
{
    if ( isnull () || isarray () || isobject () )
        return size () == 0u;
    else
        return false;
}


bool
value::operator! () const
{
    return isnull ();
}


void
value::clear ()
{
    json_assert ( type_ == nullvalue  ||  type_ == arrayvalue  || type_ == objectvalue );

    switch ( type_ )
    {
#ifndef json_value_use_internal_map

    case arrayvalue:
    case objectvalue:
        value_.map_->clear ();
        break;
#else

    case arrayvalue:
        value_.array_->clear ();
        break;

    case objectvalue:
        value_.map_->clear ();
        break;
#endif

    default:
        break;
    }
}

void
value::resize ( uint newsize )
{
    json_assert ( type_ == nullvalue  ||  type_ == arrayvalue );

    if ( type_ == nullvalue )
        *this = value ( arrayvalue );

#ifndef json_value_use_internal_map
    uint oldsize = size ();

    if ( newsize == 0 )
        clear ();
    else if ( newsize > oldsize )
        (*this)[ newsize - 1 ];
    else
    {
        for ( uint index = newsize; index < oldsize; ++index )
            value_.map_->erase ( index );

        assert ( size () == newsize );
    }

#else
    value_.array_->resize ( newsize );
#endif
}


value&
value::operator[] ( uint index )
{
    json_assert ( type_ == nullvalue  ||  type_ == arrayvalue );

    if ( type_ == nullvalue )
        *this = value ( arrayvalue );

#ifndef json_value_use_internal_map
    czstring key ( index );
    objectvalues::iterator it = value_.map_->lower_bound ( key );

    if ( it != value_.map_->end ()  &&  (*it).first == key )
        return (*it).second;

    objectvalues::value_type defaultvalue ( key, null );
    it = value_.map_->insert ( it, defaultvalue );
    return (*it).second;
#else
    return value_.array_->resolvereference ( index );
#endif
}


const value&
value::operator[] ( uint index ) const
{
    json_assert ( type_ == nullvalue  ||  type_ == arrayvalue );

    if ( type_ == nullvalue )
        return null;

#ifndef json_value_use_internal_map
    czstring key ( index );
    objectvalues::const_iterator it = value_.map_->find ( key );

    if ( it == value_.map_->end () )
        return null;

    return (*it).second;
#else
    value* value = value_.array_->find ( index );
    return value ? *value : null;
#endif
}


value&
value::operator[] ( const char* key )
{
    return resolvereference ( key, false );
}


value&
value::resolvereference ( const char* key,
                          bool isstatic )
{
    json_assert ( type_ == nullvalue  ||  type_ == objectvalue );

    if ( type_ == nullvalue )
        *this = value ( objectvalue );

#ifndef json_value_use_internal_map
    czstring actualkey ( key, isstatic ? czstring::noduplication
                         : czstring::duplicateoncopy );
    objectvalues::iterator it = value_.map_->lower_bound ( actualkey );

    if ( it != value_.map_->end ()  &&  (*it).first == actualkey )
        return (*it).second;

    objectvalues::value_type defaultvalue ( actualkey, null );
    it = value_.map_->insert ( it, defaultvalue );
    value& value = (*it).second;
    return value;
#else
    return value_.map_->resolvereference ( key, isstatic );
#endif
}


value
value::get ( uint index,
             const value& defaultvalue ) const
{
    const value* value = & ((*this)[index]);
    return value == &null ? defaultvalue : *value;
}


bool
value::isvalidindex ( uint index ) const
{
    return index < size ();
}



const value&
value::operator[] ( const char* key ) const
{
    json_assert ( type_ == nullvalue  ||  type_ == objectvalue );

    if ( type_ == nullvalue )
        return null;

#ifndef json_value_use_internal_map
    czstring actualkey ( key, czstring::noduplication );
    objectvalues::const_iterator it = value_.map_->find ( actualkey );

    if ( it == value_.map_->end () )
        return null;

    return (*it).second;
#else
    const value* value = value_.map_->find ( key );
    return value ? *value : null;
#endif
}


value&
value::operator[] ( std::string const& key )
{
    return (*this)[ key.c_str () ];
}


const value&
value::operator[] ( std::string const& key ) const
{
    return (*this)[ key.c_str () ];
}

value&
value::operator[] ( const staticstring& key )
{
    return resolvereference ( key, true );
}


# ifdef json_use_cpptl
value&
value::operator[] ( const cpptl::conststring& key )
{
    return (*this)[ key.c_str () ];
}


const value&
value::operator[] ( const cpptl::conststring& key ) const
{
    return (*this)[ key.c_str () ];
}
# endif


value&
value::append ( const value& value )
{
    return (*this)[size ()] = value;
}


value
value::get ( const char* key,
             const value& defaultvalue ) const
{
    const value* value = & ((*this)[key]);
    return value == &null ? defaultvalue : *value;
}


value
value::get ( std::string const& key,
             const value& defaultvalue ) const
{
    return get ( key.c_str (), defaultvalue );
}

value
value::removemember ( const char* key )
{
    json_assert ( type_ == nullvalue  ||  type_ == objectvalue );

    if ( type_ == nullvalue )
        return null;

#ifndef json_value_use_internal_map
    czstring actualkey ( key, czstring::noduplication );
    objectvalues::iterator it = value_.map_->find ( actualkey );

    if ( it == value_.map_->end () )
        return null;

    value old (it->second);
    value_.map_->erase (it);
    return old;
#else
    value* value = value_.map_->find ( key );

    if (value)
    {
        value old (*value);
        value_.map_.remove ( key );
        return old;
    }
    else
    {
        return null;
    }

#endif
}

value
value::removemember ( std::string const& key )
{
    return removemember ( key.c_str () );
}

# ifdef json_use_cpptl
value
value::get ( const cpptl::conststring& key,
             const value& defaultvalue ) const
{
    return get ( key.c_str (), defaultvalue );
}
# endif

bool
value::ismember ( const char* key ) const
{
    const value* value = & ((*this)[key]);
    return value != &null;
}


bool
value::ismember ( std::string const& key ) const
{
    return ismember ( key.c_str () );
}


# ifdef json_use_cpptl
bool
value::ismember ( const cpptl::conststring& key ) const
{
    return ismember ( key.c_str () );
}
#endif

value::members
value::getmembernames () const
{
    json_assert ( type_ == nullvalue  ||  type_ == objectvalue );

    if ( type_ == nullvalue )
        return value::members ();

    members members;
    members.reserve ( value_.map_->size () );
#ifndef json_value_use_internal_map
    objectvalues::const_iterator it = value_.map_->begin ();
    objectvalues::const_iterator itend = value_.map_->end ();

    for ( ; it != itend; ++it )
        members.push_back ( std::string ( (*it).first.c_str () ) );

#else
    valueinternalmap::iteratorstate it;
    valueinternalmap::iteratorstate itend;
    value_.map_->makebeginiterator ( it );
    value_.map_->makeenditerator ( itend );

    for ( ; !valueinternalmap::equals ( it, itend ); valueinternalmap::increment (it) )
        members.push_back ( std::string ( valueinternalmap::key ( it ) ) );

#endif
    return members;
}
//
//# ifdef json_use_cpptl
//enummembernames
//value::enummembernames() const
//{
//   if ( type_ == objectvalue )
//   {
//      return cpptl::enum::any(  cpptl::enum::transform(
//         cpptl::enum::keys( *(value_.map_), cpptl::type<const czstring &>() ),
//         membernamestransform() ) );
//   }
//   return enummembernames();
//}
//
//
//enumvalues
//value::enumvalues() const
//{
//   if ( type_ == objectvalue  ||  type_ == arrayvalue )
//      return cpptl::enum::anyvalues( *(value_.map_),
//                                     cpptl::type<const value &>() );
//   return enumvalues();
//}
//
//# endif


bool
value::isnull () const
{
    return type_ == nullvalue;
}


bool
value::isbool () const
{
    return type_ == booleanvalue;
}


bool
value::isint () const
{
    return type_ == intvalue;
}


bool
value::isuint () const
{
    return type_ == uintvalue;
}


bool
value::isintegral () const
{
    return type_ == intvalue
           ||  type_ == uintvalue
           ||  type_ == booleanvalue;
}


bool
value::isdouble () const
{
    return type_ == realvalue;
}


bool
value::isnumeric () const
{
    return isintegral () || isdouble ();
}


bool
value::isstring () const
{
    return type_ == stringvalue;
}


bool
value::isarray () const
{
    return type_ == nullvalue  ||  type_ == arrayvalue;
}


bool
value::isobject () const
{
    return type_ == nullvalue  ||  type_ == objectvalue;
}


void
value::setcomment ( const char* comment,
                    commentplacement placement )
{
    if ( !comments_ )
        comments_ = new commentinfo[numberofcommentplacement];

    comments_[placement].setcomment ( comment );
}


void
value::setcomment ( std::string const& comment,
                    commentplacement placement )
{
    setcomment ( comment.c_str (), placement );
}


bool
value::hascomment ( commentplacement placement ) const
{
    return comments_ != 0  &&  comments_[placement].comment_ != 0;
}

std::string
value::getcomment ( commentplacement placement ) const
{
    if ( hascomment (placement) )
        return comments_[placement].comment_;

    return "";
}


std::string
value::tostyledstring () const
{
    styledwriter writer;
    return writer.write ( *this );
}


value::const_iterator
value::begin () const
{
    switch ( type_ )
    {
#ifdef json_value_use_internal_map

    case arrayvalue:
        if ( value_.array_ )
        {
            valueinternalarray::iteratorstate it;
            value_.array_->makebeginiterator ( it );
            return const_iterator ( it );
        }

        break;

    case objectvalue:
        if ( value_.map_ )
        {
            valueinternalmap::iteratorstate it;
            value_.map_->makebeginiterator ( it );
            return const_iterator ( it );
        }

        break;
#else

    case arrayvalue:
    case objectvalue:
        if ( value_.map_ )
            return const_iterator ( value_.map_->begin () );

        break;
#endif

    default:
        break;
    }

    return const_iterator ();
}

value::const_iterator
value::end () const
{
    switch ( type_ )
    {
#ifdef json_value_use_internal_map

    case arrayvalue:
        if ( value_.array_ )
        {
            valueinternalarray::iteratorstate it;
            value_.array_->makeenditerator ( it );
            return const_iterator ( it );
        }

        break;

    case objectvalue:
        if ( value_.map_ )
        {
            valueinternalmap::iteratorstate it;
            value_.map_->makeenditerator ( it );
            return const_iterator ( it );
        }

        break;
#else

    case arrayvalue:
    case objectvalue:
        if ( value_.map_ )
            return const_iterator ( value_.map_->end () );

        break;
#endif

    default:
        break;
    }

    return const_iterator ();
}


value::iterator
value::begin ()
{
    switch ( type_ )
    {
#ifdef json_value_use_internal_map

    case arrayvalue:
        if ( value_.array_ )
        {
            valueinternalarray::iteratorstate it;
            value_.array_->makebeginiterator ( it );
            return iterator ( it );
        }

        break;

    case objectvalue:
        if ( value_.map_ )
        {
            valueinternalmap::iteratorstate it;
            value_.map_->makebeginiterator ( it );
            return iterator ( it );
        }

        break;
#else

    case arrayvalue:
    case objectvalue:
        if ( value_.map_ )
            return iterator ( value_.map_->begin () );

        break;
#endif

    default:
        break;
    }

    return iterator ();
}

value::iterator
value::end ()
{
    switch ( type_ )
    {
#ifdef json_value_use_internal_map

    case arrayvalue:
        if ( value_.array_ )
        {
            valueinternalarray::iteratorstate it;
            value_.array_->makeenditerator ( it );
            return iterator ( it );
        }

        break;

    case objectvalue:
        if ( value_.map_ )
        {
            valueinternalmap::iteratorstate it;
            value_.map_->makeenditerator ( it );
            return iterator ( it );
        }

        break;
#else

    case arrayvalue:
    case objectvalue:
        if ( value_.map_ )
            return iterator ( value_.map_->end () );

        break;
#endif

    default:
        break;
    }

    return iterator ();
}


// class pathargument
// //////////////////////////////////////////////////////////////////

pathargument::pathargument ()
    : kind_ ( kindnone )
{
}


pathargument::pathargument ( value::uint index )
    : index_ ( index )
    , kind_ ( kindindex )
{
}


pathargument::pathargument ( const char* key )
    : key_ ( key )
    , kind_ ( kindkey )
{
}


pathargument::pathargument ( std::string const& key )
    : key_ ( key.c_str () )
    , kind_ ( kindkey )
{
}

// class path
// //////////////////////////////////////////////////////////////////

path::path ( std::string const& path,
             const pathargument& a1,
             const pathargument& a2,
             const pathargument& a3,
             const pathargument& a4,
             const pathargument& a5 )
{
    inargs in;
    in.push_back ( &a1 );
    in.push_back ( &a2 );
    in.push_back ( &a3 );
    in.push_back ( &a4 );
    in.push_back ( &a5 );
    makepath ( path, in );
}


void
path::makepath ( std::string const& path,
                 const inargs& in )
{
    const char* current = path.c_str ();
    const char* end = current + path.length ();
    inargs::const_iterator itinarg = in.begin ();

    while ( current != end )
    {
        if ( *current == '[' )
        {
            ++current;

            if ( *current == '%' )
                addpathinarg ( path, in, itinarg, pathargument::kindindex );
            else
            {
                value::uint index = 0;

                for ( ; current != end && *current >= '0'  &&  *current <= '9'; ++current )
                    index = index * 10 + value::uint (*current - '0');

                args_.push_back ( index );
            }

            if ( current == end  ||  *current++ != ']' )
                invalidpath ( path, int (current - path.c_str ()) );
        }
        else if ( *current == '%' )
        {
            addpathinarg ( path, in, itinarg, pathargument::kindkey );
            ++current;
        }
        else if ( *current == '.' )
        {
            ++current;
        }
        else
        {
            const char* beginname = current;

            while ( current != end  &&  !strchr ( "[.", *current ) )
                ++current;

            args_.push_back ( std::string ( beginname, current ) );
        }
    }
}


void
path::addpathinarg ( std::string const& path,
                     const inargs& in,
                     inargs::const_iterator& itinarg,
                     pathargument::kind kind )
{
    if ( itinarg == in.end () )
    {
        // error: missing argument %d
    }
    else if ( (*itinarg)->kind_ != kind )
    {
        // error: bad argument type
    }
    else
    {
        args_.push_back ( **itinarg );
    }
}


void
path::invalidpath ( std::string const& path,
                    int location )
{
    // error: invalid path.
}


const value&
path::resolve ( const value& root ) const
{
    const value* node = &root;

    for ( args::const_iterator it = args_.begin (); it != args_.end (); ++it )
    {
        const pathargument& arg = *it;

        if ( arg.kind_ == pathargument::kindindex )
        {
            if ( !node->isarray ()  ||  node->isvalidindex ( arg.index_ ) )
            {
                // error: unable to resolve path (array value expected at position...
            }

            node = & ((*node)[arg.index_]);
        }
        else if ( arg.kind_ == pathargument::kindkey )
        {
            if ( !node->isobject () )
            {
                // error: unable to resolve path (object value expected at position...)
            }

            node = & ((*node)[arg.key_]);

            if ( node == &value::null )
            {
                // error: unable to resolve path (object has no member named '' at position...)
            }
        }
    }

    return *node;
}


value
path::resolve ( const value& root,
                const value& defaultvalue ) const
{
    const value* node = &root;

    for ( args::const_iterator it = args_.begin (); it != args_.end (); ++it )
    {
        const pathargument& arg = *it;

        if ( arg.kind_ == pathargument::kindindex )
        {
            if ( !node->isarray ()  ||  node->isvalidindex ( arg.index_ ) )
                return defaultvalue;

            node = & ((*node)[arg.index_]);
        }
        else if ( arg.kind_ == pathargument::kindkey )
        {
            if ( !node->isobject () )
                return defaultvalue;

            node = & ((*node)[arg.key_]);

            if ( node == &value::null )
                return defaultvalue;
        }
    }

    return *node;
}


value&
path::make ( value& root ) const
{
    value* node = &root;

    for ( args::const_iterator it = args_.begin (); it != args_.end (); ++it )
    {
        const pathargument& arg = *it;

        if ( arg.kind_ == pathargument::kindindex )
        {
            if ( !node->isarray () )
            {
                // error: node is not an array at position ...
            }

            node = & ((*node)[arg.index_]);
        }
        else if ( arg.kind_ == pathargument::kindkey )
        {
            if ( !node->isobject () )
            {
                // error: node is not an object at position...
            }

            node = & ((*node)[arg.key_]);
        }
    }

    return *node;
}


} // namespace json
