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

#ifndef cpptl_json_h_included
#define cpptl_json_h_included

#include <ripple/json/json_config.h>
#include <ripple/json/json_forwards.h>
#include <beast/strings/string.h>
#include <functional>
#include <map>
#include <vector>

/** \brief json (javascript object notation).
 */
namespace json
{

/** \brief type of the value held by a value object.
 */
enum valuetype
{
    nullvalue = 0, ///< 'null' value
    intvalue,      ///< signed integer value
    uintvalue,     ///< unsigned integer value
    realvalue,     ///< double value
    stringvalue,   ///< utf-8 string value
    booleanvalue,  ///< bool value
    arrayvalue,    ///< array value (ordered list)
    objectvalue    ///< object value (collection of name/value pairs).
};

enum commentplacement
{
    commentbefore = 0,        ///< a comment placed on the line before a value
    commentafteronsameline,   ///< a comment just after a value on the same line
    commentafter,             ///< a comment on the line after a value (only make sense for root value)
    numberofcommentplacement
};

//# ifdef json_use_cpptl
//   typedef cpptl::anyenumerator<const char *> enummembernames;
//   typedef cpptl::anyenumerator<const value &> enumvalues;
//# endif

/** \brief lightweight wrapper to tag static string.
 *
 * value constructor and objectvalue member assignement takes advantage of the
 * staticstring and avoid the cost of string duplication when storing the
 * string or the member name.
 *
 * example of usage:
 * \code
 * json::value avalue( staticstring("some text") );
 * json::value object;
 * static const staticstring code("code");
 * object[code] = 1234;
 * \endcode
 */
class json_api staticstring
{
public:
    explicit staticstring ( const char* czstring )
        : str_ ( czstring )
    {
    }

    operator const char* () const
    {
        return str_;
    }

    const char* c_str () const
    {
        return str_;
    }

private:
    const char* str_;
};

/** \brief represents a <a href="http://www.json.org">json</a> value.
 *
 * this class is a discriminated union wrapper that can represents a:
 * - signed integer [range: value::minint - value::maxint]
 * - unsigned integer (range: 0 - value::maxuint)
 * - double
 * - utf-8 string
 * - boolean
 * - 'null'
 * - an ordered list of value
 * - collection of name/value pairs (javascript object)
 *
 * the type of the held value is represented by a #valuetype and
 * can be obtained using type().
 *
 * values of an #objectvalue or #arrayvalue can be accessed using operator[]() methods.
 * non const methods will automatically create the a #nullvalue element
 * if it does not exist.
 * the sequence of an #arrayvalue will be automatically resize and initialized
 * with #nullvalue. resize() can be used to enlarge or truncate an #arrayvalue.
 *
 * the get() methods can be used to obtanis default value in the case the required element
 * does not exist.
 *
 * it is possible to iterate over the list of a #objectvalue values using
 * the getmembernames() method.
 */
class json_api value
{
    friend class valueiteratorbase;
# ifdef json_value_use_internal_map
    friend class valueinternallink;
    friend class valueinternalmap;
# endif
public:
    typedef std::vector<std::string> members;
    typedef valueiterator iterator;
    typedef valueconstiterator const_iterator;
    typedef json::uint uint;
    typedef json::int int;
    typedef uint arrayindex;

    static const value null;
    static const int minint;
    static const int maxint;
    static const uint maxuint;

private:
#ifndef jsoncpp_doc_exclude_implementation
# ifndef json_value_use_internal_map
    class czstring
    {
    public:
        enum duplicationpolicy
        {
            noduplication = 0,
            duplicate,
            duplicateoncopy
        };
        czstring ( int index );
        czstring ( const char* cstr, duplicationpolicy allocate );
        czstring ( const czstring& other );
        ~czstring ();
        czstring& operator = ( const czstring& other );
        bool operator< ( const czstring& other ) const;
        bool operator== ( const czstring& other ) const;
        int index () const;
        const char* c_str () const;
        bool isstaticstring () const;
    private:
        void swap ( czstring& other );
        const char* cstr_;
        int index_;
    };

public:
#  ifndef json_use_cpptl_smallmap
    typedef std::map<czstring, value> objectvalues;
#  else
    typedef cpptl::smallmap<czstring, value> objectvalues;
#  endif // ifndef json_use_cpptl_smallmap
# endif // ifndef json_value_use_internal_map
#endif // ifndef jsoncpp_doc_exclude_implementation

public:
    /** \brief create a default value of the given type.

      this is a very useful constructor.
      to create an empty array, pass arrayvalue.
      to create an empty object, pass objectvalue.
      another value can then be set to this one by assignment.
    this is useful since clear() and resize() will not alter types.

           examples:
    \code
    json::value null_value; // null
    json::value arr_value(json::arrayvalue); // []
    json::value obj_value(json::objectvalue); // {}
    \endcode
         */
    value ( valuetype type = nullvalue );
    value ( int value );
    value ( uint value );
    value ( double value );
    value ( const char* value );
    value ( const char* beginvalue, const char* endvalue );
    /** \brief constructs a value from a static string.

     * like other value string constructor but do not duplicate the string for
     * internal storage. the given string must remain alive after the call to this
     * constructor.
     * example of usage:
     * \code
     * json::value avalue( staticstring("some text") );
     * \endcode
     */
    value ( const staticstring& value );
    value ( std::string const& value );
    // vfalco deprecated avoid this conversion
    value (beast::string const& beaststring);
# ifdef json_use_cpptl
    value ( const cpptl::conststring& value );
# endif
    value ( bool value );
    value ( const value& other );
    ~value ();

    value& operator= ( const value& other );

    value ( value&& other );
    value& operator= ( value&& other );

    /// swap values.
    /// \note currently, comments are intentionally not swapped, for
    /// both logic and efficiency.
    void swap ( value& other );

    valuetype type () const;

    bool operator < ( const value& other ) const;
    bool operator <= ( const value& other ) const;
    bool operator >= ( const value& other ) const;
    bool operator > ( const value& other ) const;

    bool operator == ( const value& other ) const;
    bool operator != ( const value& other ) const;

    int compare ( const value& other );

    const char* ascstring () const;
    std::string asstring () const;
# ifdef json_use_cpptl
    cpptl::conststring asconststring () const;
# endif
    int asint () const;
    uint asuint () const;
    double asdouble () const;
    bool asbool () const;

    bool isnull () const;
    bool isbool () const;
    bool isint () const;
    bool isuint () const;
    bool isintegral () const;
    bool isdouble () const;
    bool isnumeric () const;
    bool isstring () const;
    bool isarray () const;
    bool isobject () const;

    bool isconvertibleto ( valuetype other ) const;

    /// number of values in array or object
    uint size () const;

    /// \brief return true if empty array, empty object, or null;
    /// otherwise, false.
    bool empty () const;

    /// return isnull()
    bool operator! () const;

    /// remove all object members and array elements.
    /// \pre type() is arrayvalue, objectvalue, or nullvalue
    /// \post type() is unchanged
    void clear ();

    /// resize the array to size elements.
    /// new elements are initialized to null.
    /// may only be called on nullvalue or arrayvalue.
    /// \pre type() is arrayvalue or nullvalue
    /// \post type() is arrayvalue
    void resize ( uint size );

    /// access an array element (zero based index ).
    /// if the array contains less than index element, then null value are inserted
    /// in the array so that its size is index+1.
    /// (you may need to say 'value[0u]' to get your compiler to distinguish
    ///  this from the operator[] which takes a string.)
    value& operator[] ( uint index );
    /// access an array element (zero based index )
    /// (you may need to say 'value[0u]' to get your compiler to distinguish
    ///  this from the operator[] which takes a string.)
    const value& operator[] ( uint index ) const;
    /// if the array contains at least index+1 elements, returns the element value,
    /// otherwise returns defaultvalue.
    value get ( uint index,
                const value& defaultvalue ) const;
    /// return true if index < size().
    bool isvalidindex ( uint index ) const;
    /// \brief append value to array at the end.
    ///
    /// equivalent to jsonvalue[jsonvalue.size()] = value;
    value& append ( const value& value );

    /// access an object value by name, create a null member if it does not exist.
    value& operator[] ( const char* key );
    /// access an object value by name, returns null if there is no member with that name.
    const value& operator[] ( const char* key ) const;
    /// access an object value by name, create a null member if it does not exist.
    value& operator[] ( std::string const& key );
    /// access an object value by name, returns null if there is no member with that name.
    const value& operator[] ( std::string const& key ) const;
    /** \brief access an object value by name, create a null member if it does not exist.

     * if the object as no entry for that name, then the member name used to store
     * the new entry is not duplicated.
     * example of use:
     * \code
     * json::value object;
     * static const staticstring code("code");
     * object[code] = 1234;
     * \endcode
     */
    value& operator[] ( const staticstring& key );
# ifdef json_use_cpptl
    /// access an object value by name, create a null member if it does not exist.
    value& operator[] ( const cpptl::conststring& key );
    /// access an object value by name, returns null if there is no member with that name.
    const value& operator[] ( const cpptl::conststring& key ) const;
# endif
    /// return the member named key if it exist, defaultvalue otherwise.
    value get ( const char* key,
                const value& defaultvalue ) const;
    /// return the member named key if it exist, defaultvalue otherwise.
    value get ( std::string const& key,
                const value& defaultvalue ) const;
# ifdef json_use_cpptl
    /// return the member named key if it exist, defaultvalue otherwise.
    value get ( const cpptl::conststring& key,
                const value& defaultvalue ) const;
# endif
    /// \brief remove and return the named member.
    ///
    /// do nothing if it did not exist.
    /// \return the removed value, or null.
    /// \pre type() is objectvalue or nullvalue
    /// \post type() is unchanged
    value removemember ( const char* key );
    /// same as removemember(const char*)
    value removemember ( std::string const& key );

    /// return true if the object has a member named key.
    bool ismember ( const char* key ) const;
    /// return true if the object has a member named key.
    bool ismember ( std::string const& key ) const;
# ifdef json_use_cpptl
    /// return true if the object has a member named key.
    bool ismember ( const cpptl::conststring& key ) const;
# endif

    /// \brief return a list of the member names.
    ///
    /// if null, return an empty list.
    /// \pre type() is objectvalue or nullvalue
    /// \post if type() was nullvalue, it remains nullvalue
    members getmembernames () const;

    //# ifdef json_use_cpptl
    //      enummembernames enummembernames() const;
    //      enumvalues enumvalues() const;
    //# endif

    /// comments must be //... or /* ... */
    void setcomment ( const char* comment,
                      commentplacement placement );
    /// comments must be //... or /* ... */
    void setcomment ( std::string const& comment,
                      commentplacement placement );
    bool hascomment ( commentplacement placement ) const;
    /// include delimiters and embedded newlines.
    std::string getcomment ( commentplacement placement ) const;

    std::string tostyledstring () const;

    const_iterator begin () const;
    const_iterator end () const;

    iterator begin ();
    iterator end ();

private:
    value& resolvereference ( const char* key,
                              bool isstatic );

# ifdef json_value_use_internal_map
    inline bool isitemavailable () const
    {
        return itemisused_ == 0;
    }

    inline void setitemused ( bool isused = true )
    {
        itemisused_ = isused ? 1 : 0;
    }

    inline bool ismembernamestatic () const
    {
        return membernameisstatic_ == 0;
    }

    inline void setmembernameisstatic ( bool isstatic )
    {
        membernameisstatic_ = isstatic ? 1 : 0;
    }
# endif // # ifdef json_value_use_internal_map

private:
    struct commentinfo
    {
        commentinfo ();
        ~commentinfo ();

        void setcomment ( const char* text );

        char* comment_;
    };

    //struct membernamestransform
    //{
    //   typedef const char *result_type;
    //   const char *operator()( const czstring &name ) const
    //   {
    //      return name.c_str();
    //   }
    //};

    union valueholder
    {
        int int_;
        uint uint_;
        double real_;
        bool bool_;
        char* string_;
# ifdef json_value_use_internal_map
        valueinternalarray* array_;
        valueinternalmap* map_;
#else
        objectvalues* map_;
# endif
    } value_;
    valuetype type_ : 8;
    int allocated_ : 1;     // notes: if declared as bool, bitfield is useless.
# ifdef json_value_use_internal_map
    unsigned int itemisused_ : 1;      // used by the valueinternalmap container.
    int membernameisstatic_ : 1;       // used by the valueinternalmap container.
# endif
    commentinfo* comments_;
};


/** \brief experimental and untested: represents an element of the "path" to access a node.
 */
class pathargument
{
public:
    friend class path;

    pathargument ();
    pathargument ( uint index );
    pathargument ( const char* key );
    pathargument ( std::string const& key );

private:
    enum kind
    {
        kindnone = 0,
        kindindex,
        kindkey
    };
    std::string key_;
    uint index_;
    kind kind_;
};

/** \brief experimental and untested: represents a "path" to access a node.
 *
 * syntax:
 * - "." => root node
 * - ".[n]" => elements at index 'n' of root node (an array value)
 * - ".name" => member named 'name' of root node (an object value)
 * - ".name1.name2.name3"
 * - ".[0][1][2].name1[3]"
 * - ".%" => member name is provided as parameter
 * - ".[%]" => index is provied as parameter
 */
class path
{
public:
    path ( std::string const& path,
           const pathargument& a1 = pathargument (),
           const pathargument& a2 = pathargument (),
           const pathargument& a3 = pathargument (),
           const pathargument& a4 = pathargument (),
           const pathargument& a5 = pathargument () );

    const value& resolve ( const value& root ) const;
    value resolve ( const value& root,
                    const value& defaultvalue ) const;
    /// creates the "path" to access the specified node and returns a reference on the node.
    value& make ( value& root ) const;

private:
    typedef std::vector<const pathargument*> inargs;
    typedef std::vector<pathargument> args;

    void makepath ( std::string const& path,
                    const inargs& in );
    void addpathinarg ( std::string const& path,
                        const inargs& in,
                        inargs::const_iterator& itinarg,
                        pathargument::kind kind );
    void invalidpath ( std::string const& path,
                       int location );

    args args_;
};

/** \brief experimental do not use: allocator to customize member name and string value memory management done by value.
 *
 * - makemembername() and releasemembername() are called to respectively duplicate and
 *   free an json::objectvalue member name.
 * - duplicatestringvalue() and releasestringvalue() are called similarly to
 *   duplicate and free a json::stringvalue value.
 */
class valueallocator
{
public:
    enum { unknown = (unsigned) - 1 };

    virtual ~valueallocator ();

    virtual char* makemembername ( const char* membername ) = 0;
    virtual void releasemembername ( char* membername ) = 0;
    virtual char* duplicatestringvalue ( const char* value,
                                         unsigned int length = unknown ) = 0;
    virtual void releasestringvalue ( char* value ) = 0;
};

#ifdef json_value_use_internal_map
/** \brief allocator to customize value internal map.
 * below is an example of a simple implementation (default implementation actually
 * use memory pool for speed).
 * \code
   class defaultvaluemapallocator : public valuemapallocator
   {
   public: // overridden from valuemapallocator
      virtual valueinternalmap *newmap()
      {
         return new valueinternalmap();
      }

      virtual valueinternalmap *newmapcopy( const valueinternalmap &other )
      {
         return new valueinternalmap( other );
      }

      virtual void destructmap( valueinternalmap *map )
      {
         delete map;
      }

      virtual valueinternallink *allocatemapbuckets( unsigned int size )
      {
         return new valueinternallink[size];
      }

      virtual void releasemapbuckets( valueinternallink *links )
      {
         delete [] links;
      }

      virtual valueinternallink *allocatemaplink()
      {
         return new valueinternallink();
      }

      virtual void releasemaplink( valueinternallink *link )
      {
         delete link;
      }
   };
 * \endcode
 */
class json_api valuemapallocator
{
public:
    virtual ~valuemapallocator ();
    virtual valueinternalmap* newmap () = 0;
    virtual valueinternalmap* newmapcopy ( const valueinternalmap& other ) = 0;
    virtual void destructmap ( valueinternalmap* map ) = 0;
    virtual valueinternallink* allocatemapbuckets ( unsigned int size ) = 0;
    virtual void releasemapbuckets ( valueinternallink* links ) = 0;
    virtual valueinternallink* allocatemaplink () = 0;
    virtual void releasemaplink ( valueinternallink* link ) = 0;
};

/** \brief valueinternalmap hash-map bucket chain link (for internal use only).
 * \internal previous_ & next_ allows for bidirectional traversal.
 */
class json_api valueinternallink
{
public:
    enum { itemperlink = 6 };  // sizeof(valueinternallink) = 128 on 32 bits architecture.
    enum internalflags
    {
        flagavailable = 0,
        flagused = 1
    };

    valueinternallink ();

    ~valueinternallink ();

    value items_[itemperlink];
    char* keys_[itemperlink];
    valueinternallink* previous_;
    valueinternallink* next_;
};


/** \brief a linked page based hash-table implementation used internally by value.
 * \internal valueinternalmap is a tradional bucket based hash-table, with a linked
 * list in each bucket to handle collision. there is an addional twist in that
 * each node of the collision linked list is a page containing a fixed amount of
 * value. this provides a better compromise between memory usage and speed.
 *
 * each bucket is made up of a chained list of valueinternallink. the last
 * link of a given bucket can be found in the 'previous_' field of the following bucket.
 * the last link of the last bucket is stored in taillink_ as it has no following bucket.
 * only the last link of a bucket may contains 'available' item. the last link always
 * contains at least one element unless is it the bucket one very first link.
 */
class json_api valueinternalmap
{
    friend class valueiteratorbase;
    friend class value;
public:
    typedef unsigned int hashkey;
    typedef unsigned int bucketindex;

# ifndef jsoncpp_doc_exclude_implementation
    struct iteratorstate
    {
        iteratorstate ()
            : map_ (0)
            , link_ (0)
            , itemindex_ (0)
            , bucketindex_ (0)
        {
        }
        valueinternalmap* map_;
        valueinternallink* link_;
        bucketindex itemindex_;
        bucketindex bucketindex_;
    };
# endif // ifndef jsoncpp_doc_exclude_implementation

    valueinternalmap ();
    valueinternalmap ( const valueinternalmap& other );
    valueinternalmap& operator = ( const valueinternalmap& other );
    ~valueinternalmap ();

    void swap ( valueinternalmap& other );

    bucketindex size () const;

    void clear ();

    bool reservedelta ( bucketindex growth );

    bool reserve ( bucketindex newitemcount );

    const value* find ( const char* key ) const;

    value* find ( const char* key );

    value& resolvereference ( const char* key,
                              bool isstatic );

    void remove ( const char* key );

    void doactualremove ( valueinternallink* link,
                          bucketindex index,
                          bucketindex bucketindex );

    valueinternallink*& getlastlinkinbucket ( bucketindex bucketindex );

    value& setnewitem ( const char* key,
                        bool isstatic,
                        valueinternallink* link,
                        bucketindex index );

    value& unsafeadd ( const char* key,
                       bool isstatic,
                       hashkey hashedkey );

    hashkey hash ( const char* key ) const;

    int compare ( const valueinternalmap& other ) const;

private:
    void makebeginiterator ( iteratorstate& it ) const;
    void makeenditerator ( iteratorstate& it ) const;
    static bool equals ( const iteratorstate& x, const iteratorstate& other );
    static void increment ( iteratorstate& iterator );
    static void incrementbucket ( iteratorstate& iterator );
    static void decrement ( iteratorstate& iterator );
    static const char* key ( const iteratorstate& iterator );
    static const char* key ( const iteratorstate& iterator, bool& isstatic );
    static value& value ( const iteratorstate& iterator );
    static int distance ( const iteratorstate& x, const iteratorstate& y );

private:
    valueinternallink* buckets_;
    valueinternallink* taillink_;
    bucketindex bucketssize_;
    bucketindex itemcount_;
};

/** \brief a simplified deque implementation used internally by value.
* \internal
* it is based on a list of fixed "page", each page contains a fixed number of items.
* instead of using a linked-list, a array of pointer is used for fast item look-up.
* look-up for an element is as follow:
* - compute page index: pageindex = itemindex / itemsperpage
* - look-up item in page: pages_[pageindex][itemindex % itemsperpage]
*
* insertion is amortized constant time (only the array containing the index of pointers
* need to be reallocated when items are appended).
*/
class json_api valueinternalarray
{
    friend class value;
    friend class valueiteratorbase;
public:
    enum { itemsperpage = 8 };    // should be a power of 2 for fast divide and modulo.
    typedef value::arrayindex arrayindex;
    typedef unsigned int pageindex;

# ifndef jsoncpp_doc_exclude_implementation
    struct iteratorstate // must be a pod
    {
        iteratorstate ()
            : array_ (0)
            , currentpageindex_ (0)
            , currentitemindex_ (0)
        {
        }
        valueinternalarray* array_;
        value** currentpageindex_;
        unsigned int currentitemindex_;
    };
# endif // ifndef jsoncpp_doc_exclude_implementation

    valueinternalarray ();
    valueinternalarray ( const valueinternalarray& other );
    valueinternalarray& operator = ( const valueinternalarray& other );
    ~valueinternalarray ();
    void swap ( valueinternalarray& other );

    void clear ();
    void resize ( arrayindex newsize );

    value& resolvereference ( arrayindex index );

    value* find ( arrayindex index ) const;

    arrayindex size () const;

    int compare ( const valueinternalarray& other ) const;

private:
    static bool equals ( const iteratorstate& x, const iteratorstate& other );
    static void increment ( iteratorstate& iterator );
    static void decrement ( iteratorstate& iterator );
    static value& dereference ( const iteratorstate& iterator );
    static value& unsafedereference ( const iteratorstate& iterator );
    static int distance ( const iteratorstate& x, const iteratorstate& y );
    static arrayindex indexof ( const iteratorstate& iterator );
    void makebeginiterator ( iteratorstate& it ) const;
    void makeenditerator ( iteratorstate& it ) const;
    void makeiterator ( iteratorstate& it, arrayindex index ) const;

    void makeindexvalid ( arrayindex index );

    value** pages_;
    arrayindex size_;
    pageindex pagecount_;
};

/** \brief experimental: do not use. allocator to customize value internal array.
 * below is an example of a simple implementation (actual implementation use
 * memory pool).
   \code
class defaultvaluearrayallocator : public valuearrayallocator
{
public: // overridden from valuearrayallocator
virtual ~defaultvaluearrayallocator()
{
}

virtual valueinternalarray *newarray()
{
   return new valueinternalarray();
}

virtual valueinternalarray *newarraycopy( const valueinternalarray &other )
{
   return new valueinternalarray( other );
}

virtual void destruct( valueinternalarray *array )
{
   delete array;
}

virtual void reallocatearraypageindex( value **&indexes,
                                       valueinternalarray::pageindex &indexcount,
                                       valueinternalarray::pageindex minnewindexcount )
{
   valueinternalarray::pageindex newindexcount = (indexcount*3)/2 + 1;
   if ( minnewindexcount > newindexcount )
      newindexcount = minnewindexcount;
   void *newindexes = realloc( indexes, sizeof(value*) * newindexcount );
   if ( !newindexes )
      throw std::bad_alloc();
   indexcount = newindexcount;
   indexes = static_cast<value **>( newindexes );
}
virtual void releasearraypageindex( value **indexes,
                                    valueinternalarray::pageindex indexcount )
{
   if ( indexes )
      free( indexes );
}

virtual value *allocatearraypage()
{
   return static_cast<value *>( malloc( sizeof(value) * valueinternalarray::itemsperpage ) );
}

virtual void releasearraypage( value *value )
{
   if ( value )
      free( value );
}
};
   \endcode
 */
class json_api valuearrayallocator
{
public:
    virtual ~valuearrayallocator ();
    virtual valueinternalarray* newarray () = 0;
    virtual valueinternalarray* newarraycopy ( const valueinternalarray& other ) = 0;
    virtual void destructarray ( valueinternalarray* array ) = 0;
    /** \brief reallocate array page index.
     * reallocates an array of pointer on each page.
     * \param indexes [input] pointer on the current index. may be \c null.
     *                [output] pointer on the new index of at least
     *                         \a minnewindexcount pages.
     * \param indexcount [input] current number of pages in the index.
     *                   [output] number of page the reallocated index can handle.
     *                            \b must be >= \a minnewindexcount.
     * \param minnewindexcount minimum number of page the new index must be able to
     *                         handle.
     */
    virtual void reallocatearraypageindex ( value**& indexes,
                                            valueinternalarray::pageindex& indexcount,
                                            valueinternalarray::pageindex minnewindexcount ) = 0;
    virtual void releasearraypageindex ( value** indexes,
                                         valueinternalarray::pageindex indexcount ) = 0;
    virtual value* allocatearraypage () = 0;
    virtual void releasearraypage ( value* value ) = 0;
};
#endif // #ifdef json_value_use_internal_map


/** \brief base class for value iterators.
 *
 */
class valueiteratorbase
{
public:
    typedef unsigned int size_t;
    typedef int difference_type;
    typedef valueiteratorbase selftype;

    valueiteratorbase ();
#ifndef json_value_use_internal_map
    explicit valueiteratorbase ( const value::objectvalues::iterator& current );
#else
    valueiteratorbase ( const valueinternalarray::iteratorstate& state );
    valueiteratorbase ( const valueinternalmap::iteratorstate& state );
#endif

    bool operator == ( const selftype& other ) const
    {
        return isequal ( other );
    }

    bool operator != ( const selftype& other ) const
    {
        return !isequal ( other );
    }

    difference_type operator - ( const selftype& other ) const
    {
        return computedistance ( other );
    }

    /// return either the index or the member name of the referenced value as a value.
    value key () const;

    /// return the index of the referenced value. -1 if it is not an arrayvalue.
    uint index () const;

    /// return the member name of the referenced value. "" if it is not an objectvalue.
    const char* membername () const;

protected:
    value& deref () const;

    void increment ();

    void decrement ();

    difference_type computedistance ( const selftype& other ) const;

    bool isequal ( const selftype& other ) const;

    void copy ( const selftype& other );

private:
#ifndef json_value_use_internal_map
    value::objectvalues::iterator current_;
    // indicates that iterator is for a null value.
    bool isnull_;
#else
    union
    {
        valueinternalarray::iteratorstate array_;
        valueinternalmap::iteratorstate map_;
    } iterator_;
    bool isarray_;
#endif
};

/** \brief const iterator for object and array value.
 *
 */
class valueconstiterator : public valueiteratorbase
{
    friend class value;
public:
    typedef unsigned int size_t;
    typedef int difference_type;
    typedef const value& reference;
    typedef const value* pointer;
    typedef valueconstiterator selftype;

    valueconstiterator ();
private:
    /*! \internal use by value to create an iterator.
     */
#ifndef json_value_use_internal_map
    explicit valueconstiterator ( const value::objectvalues::iterator& current );
#else
    valueconstiterator ( const valueinternalarray::iteratorstate& state );
    valueconstiterator ( const valueinternalmap::iteratorstate& state );
#endif
public:
    selftype& operator = ( const valueiteratorbase& other );

    selftype operator++ ( int )
    {
        selftype temp ( *this );
        ++*this;
        return temp;
    }

    selftype operator-- ( int )
    {
        selftype temp ( *this );
        --*this;
        return temp;
    }

    selftype& operator-- ()
    {
        decrement ();
        return *this;
    }

    selftype& operator++ ()
    {
        increment ();
        return *this;
    }

    reference operator * () const
    {
        return deref ();
    }
};


/** \brief iterator for object and array value.
 */
class valueiterator : public valueiteratorbase
{
    friend class value;
public:
    typedef unsigned int size_t;
    typedef int difference_type;
    typedef value& reference;
    typedef value* pointer;
    typedef valueiterator selftype;

    valueiterator ();
    valueiterator ( const valueconstiterator& other );
    valueiterator ( const valueiterator& other );
private:
    /*! \internal use by value to create an iterator.
     */
#ifndef json_value_use_internal_map
    explicit valueiterator ( const value::objectvalues::iterator& current );
#else
    valueiterator ( const valueinternalarray::iteratorstate& state );
    valueiterator ( const valueinternalmap::iteratorstate& state );
#endif
public:

    selftype& operator = ( const selftype& other );

    selftype operator++ ( int )
    {
        selftype temp ( *this );
        ++*this;
        return temp;
    }

    selftype operator-- ( int )
    {
        selftype temp ( *this );
        --*this;
        return temp;
    }

    selftype& operator-- ()
    {
        decrement ();
        return *this;
    }

    selftype& operator++ ()
    {
        increment ();
        return *this;
    }

    reference operator * () const
    {
        return deref ();
    }
};

//------------------------------------------------------------------------------

using write_t = std::function<void(void const*, std::size_t)>;

/** stream compact json to the specified function. */
void
stream (json::value const& jv, write_t write);

} // namespace json


#endif // cpptl_json_h_included
