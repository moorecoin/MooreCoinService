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

#ifndef beast_osx_objchelpers_h_included
#define beast_osx_objchelpers_h_included

namespace beast
{

/* this file contains a few helper functions that are used internally but which
   need to be kept away from the public headers because they use obj-c symbols.
*/
namespace
{
    //==============================================================================
    static inline string nsstringtobeast (nsstring* s)
    {
        return charpointer_utf8 ([s utf8string]);
    }

    static inline nsstring* beaststringtons (const string& s)
    {
        return [nsstring stringwithutf8string: s.toutf8()];
    }

    static inline nsstring* nsstringliteral (const char* const s) noexcept
    {
        return [nsstring stringwithutf8string: s];
    }

    static inline nsstring* nsemptystring() noexcept
    {
        return [nsstring string];
    }
}

//==============================================================================
template <typename objecttype>
struct nsobjectretainer
{
    inline nsobjectretainer (objecttype* o) : object (o)  { [object retain]; }
    inline ~nsobjectretainer()                            { [object release]; }

    objecttype* object;
};

//==============================================================================
template <typename superclasstype>
struct objcclass
{
    objcclass (const char* nameroot)
        : cls (objc_allocateclasspair ([superclasstype class], getrandomisedname (nameroot).toutf8(), 0))
    {
    }

    objcclass (objcclass const&);
    objcclass& operator= (objcclass const&);

    ~objcclass()
    {
        objc_disposeclasspair (cls);
    }

    void registerclass()
    {
        objc_registerclasspair (cls);
    }

    superclasstype* createinstance() const
    {
        return class_createinstance (cls, 0);
    }

    template <typename type>
    void addivar (const char* name)
    {
        bool b = class_addivar (cls, name, sizeof (type), (uint8_t) rint (log2 (sizeof (type))), @encode (type));
        bassert (b); (void) b;
    }

    template <typename functiontype>
    void addmethod (sel selector, functiontype callbackfn, const char* signature)
    {
        bool b = class_addmethod (cls, selector, (imp) callbackfn, signature);
        bassert (b); (void) b;
    }

    template <typename functiontype>
    void addmethod (sel selector, functiontype callbackfn, const char* sig1, const char* sig2)
    {
        addmethod (selector, callbackfn, (string (sig1) + sig2).toutf8());
    }

    template <typename functiontype>
    void addmethod (sel selector, functiontype callbackfn, const char* sig1, const char* sig2, const char* sig3)
    {
        addmethod (selector, callbackfn, (string (sig1) + sig2 + sig3).toutf8());
    }

    template <typename functiontype>
    void addmethod (sel selector, functiontype callbackfn, const char* sig1, const char* sig2, const char* sig3, const char* sig4)
    {
        addmethod (selector, callbackfn, (string (sig1) + sig2 + sig3 + sig4).toutf8());
    }

    void addprotocol (protocol* protocol)
    {
        bool b = class_addprotocol (cls, protocol);
        bassert (b); (void) b;
    }

    static id sendsuperclassmessage (id self, sel selector)
    {
        objc_super s = { self, [superclasstype class] };
        return objc_msgsendsuper (&s, selector);
    }

    template <typename type>
    static type getivar (id self, const char* name)
    {
        void* v = nullptr;
        object_getinstancevariable (self, name, &v);
        return static_cast <type> (v);
    }

    class cls;

private:
    static string getrandomisedname (const char* root)
    {
        return root + string::tohexstring (beast::random::getsystemrandom().nextint64());
    }
};

} // beast

#endif   // beast_osx_objchelpers_h_included
