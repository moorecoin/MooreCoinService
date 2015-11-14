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

#include <beast/unit_test/suite.h>

#include <algorithm>
#include <memory>

namespace beast {

// we need to make a shared singleton or else there are
// issues with the leak detector and order of detruction.
//
class nonexistentholder
{
public:
    static nonexistentholder* getinstance()
    {
        return sharedsingleton <nonexistentholder>::getinstance();
    }

    file file;
};

file const& file::nonexistent ()
{
    return nonexistentholder::getinstance ()->file;
}

//------------------------------------------------------------------------------

file::file (const string& fullpathname)
    : fullpath (parseabsolutepath (fullpathname))
{
}

file::file (const file& other)
    : fullpath (other.fullpath)
{
}

file file::createfilewithoutcheckingpath (const string& path) noexcept
{
    file f;
    f.fullpath = path;
    return f;
}

file& file::operator= (const string& newpath)
{
    fullpath = parseabsolutepath (newpath);
    return *this;
}

file& file::operator= (const file& other)
{
    fullpath = other.fullpath;
    return *this;
}

#if beast_compiler_supports_move_semantics
file::file (file&& other) noexcept
    : fullpath (static_cast <string&&> (other.fullpath))
{
}

file& file::operator= (file&& other) noexcept
{
    fullpath = static_cast <string&&> (other.fullpath);
    return *this;
}
#endif

//==============================================================================
string file::parseabsolutepath (const string& p)
{
    if (p.isempty())
        return string::empty;

#if beast_windows
    // windows..
    string path (p.replacecharacter ('/', '\\'));

    if (path.startswithchar (separator))
    {
        if (path[1] != separator)
        {
            /*  when you supply a raw string to the file object constructor, it must be an absolute path.
                if you're trying to parse a string that may be either a relative path or an absolute path,
                you must provide a context against which the partial path can be evaluated - you can do
                this by simply using file::getchildfile() instead of the file constructor. e.g. saying
                "file::getcurrentworkingdirectory().getchildfile (myunknownpath)" would return an absolute
                path if that's what was supplied, or would evaluate a partial path relative to the cwd.
            */
            bassertfalse;

            path = file::getcurrentworkingdirectory().getfullpathname().substring (0, 2) + path;
        }
    }
    else if (! path.containschar (':'))
    {
        /*  when you supply a raw string to the file object constructor, it must be an absolute path.
            if you're trying to parse a string that may be either a relative path or an absolute path,
            you must provide a context against which the partial path can be evaluated - you can do
            this by simply using file::getchildfile() instead of the file constructor. e.g. saying
            "file::getcurrentworkingdirectory().getchildfile (myunknownpath)" would return an absolute
            path if that's what was supplied, or would evaluate a partial path relative to the cwd.
        */
        bassertfalse;

        return file::getcurrentworkingdirectory().getchildfile (path).getfullpathname();
    }
#else
    // mac or linux..

    // yes, i know it's legal for a unix pathname to contain a backslash, but this assertion is here
    // to catch anyone who's trying to run code that was written on windows with hard-coded path names.
    // if that's why you've ended up here, use file::getchildfile() to build your paths instead.
    bassert ((! p.containschar ('\\')) || (p.indexofchar ('/') >= 0 && p.indexofchar ('/') < p.indexofchar ('\\')));

    string path (p);

    if (path.startswithchar ('~'))
    {
        if (path[1] == separator || path[1] == 0)
        {
            // expand a name of the form "~/abc"
            path = file::getspeciallocation (file::userhomedirectory).getfullpathname()
                    + path.substring (1);
        }
        else
        {
            // expand a name of type "~dave/abc"
            const string username (path.substring (1).uptofirstoccurrenceof ("/", false, false));

            if (struct passwd* const pw = getpwnam (username.toutf8()))
                path = addtrailingseparator (pw->pw_dir) + path.fromfirstoccurrenceof ("/", false, false);
        }
    }
    else if (! path.startswithchar (separator))
    {
       #if beast_debug || beast_log_assertions
        if (! (path.startswith ("./") || path.startswith ("../")))
        {
            /*  when you supply a raw string to the file object constructor, it must be an absolute path.
                if you're trying to parse a string that may be either a relative path or an absolute path,
                you must provide a context against which the partial path can be evaluated - you can do
                this by simply using file::getchildfile() instead of the file constructor. e.g. saying
                "file::getcurrentworkingdirectory().getchildfile (myunknownpath)" would return an absolute
                path if that's what was supplied, or would evaluate a partial path relative to the cwd.
            */
            bassertfalse;
        }
       #endif

        return file::getcurrentworkingdirectory().getchildfile (path).getfullpathname();
    }
#endif

    while (path.endswithchar (separator) && path != separatorstring) // careful not to turn a single "/" into an empty string.
        path = path.droplastcharacters (1);

    return path;
}

string file::addtrailingseparator (const string& path)
{
    return path.endswithchar (separator) ? path
                                         : path + separator;
}

//==============================================================================
#if beast_linux
 #define names_are_case_sensitive 1
#endif

bool file::arefilenamescasesensitive()
{
   #if names_are_case_sensitive
    return true;
   #else
    return false;
   #endif
}

static int comparefilenames (const string& name1, const string& name2) noexcept
{
   #if names_are_case_sensitive
    return name1.compare (name2);
   #else
    return name1.compareignorecase (name2);
   #endif
}

bool file::operator== (const file& other) const     { return comparefilenames (fullpath, other.fullpath) == 0; }
bool file::operator!= (const file& other) const     { return comparefilenames (fullpath, other.fullpath) != 0; }
bool file::operator<  (const file& other) const     { return comparefilenames (fullpath, other.fullpath) <  0; }
bool file::operator>  (const file& other) const     { return comparefilenames (fullpath, other.fullpath) >  0; }

//==============================================================================
bool file::setreadonly (const bool shouldbereadonly,
                        const bool applyrecursively) const
{
    bool worked = true;

    if (applyrecursively && isdirectory())
    {
        array <file> subfiles;
        findchildfiles (subfiles, file::findfilesanddirectories, false);

        for (int i = subfiles.size(); --i >= 0;)
            worked = subfiles.getreference(i).setreadonly (shouldbereadonly, true) && worked;
    }

    return setfilereadonlyinternal (shouldbereadonly) && worked;
}

bool file::deleterecursively() const
{
    bool worked = true;

    if (isdirectory())
    {
        array<file> subfiles;
        findchildfiles (subfiles, file::findfilesanddirectories, false);

        for (int i = subfiles.size(); --i >= 0;)
            worked = subfiles.getreference(i).deleterecursively() && worked;
    }

    return deletefile() && worked;
}

bool file::movefileto (const file& newfile) const
{
    if (newfile.fullpath == fullpath)
        return true;

    if (! exists())
        return false;

   #if ! names_are_case_sensitive
    if (*this != newfile)
   #endif
        if (! newfile.deletefile())
            return false;

    return moveinternal (newfile);
}

bool file::copyfileto (const file& newfile) const
{
    return (*this == newfile)
            || (exists() && newfile.deletefile() && copyinternal (newfile));
}

bool file::copydirectoryto (const file& newdirectory) const
{
    if (isdirectory() && newdirectory.createdirectory())
    {
        array<file> subfiles;
        findchildfiles (subfiles, file::findfiles, false);

        for (int i = 0; i < subfiles.size(); ++i)
            if (! subfiles.getreference(i).copyfileto (newdirectory.getchildfile (subfiles.getreference(i).getfilename())))
                return false;

        subfiles.clear();
        findchildfiles (subfiles, file::finddirectories, false);

        for (int i = 0; i < subfiles.size(); ++i)
            if (! subfiles.getreference(i).copydirectoryto (newdirectory.getchildfile (subfiles.getreference(i).getfilename())))
                return false;

        return true;
    }

    return false;
}

//==============================================================================
string file::getpathuptolastslash() const
{
    const int lastslash = fullpath.lastindexofchar (separator);

    if (lastslash > 0)
        return fullpath.substring (0, lastslash);

    if (lastslash == 0)
        return separatorstring;

    return fullpath;
}

file file::getparentdirectory() const
{
    file f;
    f.fullpath = getpathuptolastslash();
    return f;
}

//==============================================================================
string file::getfilename() const
{
    return fullpath.substring (fullpath.lastindexofchar (separator) + 1);
}

string file::getfilenamewithoutextension() const
{
    const int lastslash = fullpath.lastindexofchar (separator) + 1;
    const int lastdot   = fullpath.lastindexofchar ('.');

    if (lastdot > lastslash)
        return fullpath.substring (lastslash, lastdot);

    return fullpath.substring (lastslash);
}

bool file::isachildof (const file& potentialparent) const
{
    if (potentialparent == file::nonexistent ())
        return false;

    const string ourpath (getpathuptolastslash());

    if (comparefilenames (potentialparent.fullpath, ourpath) == 0)
        return true;

    if (potentialparent.fullpath.length() >= ourpath.length())
        return false;

    return getparentdirectory().isachildof (potentialparent);
}

int   file::hashcode() const    { return fullpath.hashcode(); }
std::int64_t file::hashcode64() const  { return fullpath.hashcode64(); }

//==============================================================================
bool file::isabsolutepath (const string& path)
{
    return path.startswithchar (separator)
           #if beast_windows
            || (path.isnotempty() && path[1] == ':');
           #else
            || path.startswithchar ('~');
           #endif
}

file file::getchildfile (string relativepath) const
{
    if (isabsolutepath (relativepath))
        return file (relativepath);

    string path (fullpath);

    // it's relative, so remove any ../ or ./ bits at the start..
    if (relativepath[0] == '.')
    {
       #if beast_windows
        relativepath = relativepath.replacecharacter ('/', '\\');
       #endif

        while (relativepath[0] == '.')
        {
            const beast_wchar secondchar = relativepath[1];

            if (secondchar == '.')
            {
                const beast_wchar thirdchar = relativepath[2];

                if (thirdchar == 0 || thirdchar == separator)
                {
                    const int lastslash = path.lastindexofchar (separator);
                    if (lastslash >= 0)
                        path = path.substring (0, lastslash);

                    relativepath = relativepath.substring (3);
                }
                else
                {
                    break;
                }
            }
            else if (secondchar == separator)
            {
                relativepath = relativepath.substring (2);
            }
            else
            {
                break;
            }
        }
    }

    return file (addtrailingseparator (path) + relativepath);
}

file file::getsiblingfile (const string& filename) const
{
    return getparentdirectory().getchildfile (filename);
}

//==============================================================================
result file::create() const
{
    if (exists())
        return result::ok();

    const file parentdir (getparentdirectory());

    if (parentdir == *this)
        return result::fail ("cannot create parent directory");

    result r (parentdir.createdirectory());

    if (r.wasok())
    {
        fileoutputstream fo (*this, 8);
        r = fo.getstatus();
    }

    return r;
}

result file::createdirectory() const
{
    if (isdirectory())
        return result::ok();

    const file parentdir (getparentdirectory());

    if (parentdir == *this)
        return result::fail ("cannot create parent directory");

    result r (parentdir.createdirectory());

    if (r.wasok())
        r = createdirectoryinternal (fullpath.trimcharactersatend (separatorstring));

    return r;
}

//==============================================================================
time file::getlastmodificationtime() const           { std::int64_t m, a, c; getfiletimesinternal (m, a, c); return time (m); }
time file::getlastaccesstime() const                 { std::int64_t m, a, c; getfiletimesinternal (m, a, c); return time (a); }
time file::getcreationtime() const                   { std::int64_t m, a, c; getfiletimesinternal (m, a, c); return time (c); }

bool file::setlastmodificationtime (time t) const    { return setfiletimesinternal (t.tomilliseconds(), 0, 0); }
bool file::setlastaccesstime (time t) const          { return setfiletimesinternal (0, t.tomilliseconds(), 0); }
bool file::setcreationtime (time t) const            { return setfiletimesinternal (0, 0, t.tomilliseconds()); }

//==============================================================================
int file::findchildfiles (array<file>& results,
                          const int whattolookfor,
                          const bool searchrecursively,
                          const string& wildcardpattern) const
{
    directoryiterator di (*this, searchrecursively, wildcardpattern, whattolookfor);

    int total = 0;
    while (di.next())
    {
        results.add (di.getfile());
        ++total;
    }

    return total;
}

int file::getnumberofchildfiles (const int whattolookfor, const string& wildcardpattern) const
{
    directoryiterator di (*this, false, wildcardpattern, whattolookfor);

    int total = 0;
    while (di.next())
        ++total;

    return total;
}

bool file::containssubdirectories() const
{
    if (! isdirectory())
        return false;

    directoryiterator di (*this, false, "*", finddirectories);
    return di.next();
}

//==============================================================================
file file::getnonexistentchildfile (const string& suggestedprefix,
                                    const string& suffix,
                                    bool putnumbersinbrackets) const
{
    file f (getchildfile (suggestedprefix + suffix));

    if (f.exists())
    {
        int number = 1;
        string prefix (suggestedprefix);

        // remove any bracketed numbers that may already be on the end..
        if (prefix.trim().endswithchar (')'))
        {
            putnumbersinbrackets = true;

            const int openbracks  = prefix.lastindexofchar ('(');
            const int closebracks = prefix.lastindexofchar (')');

            if (openbracks > 0
                 && closebracks > openbracks
                 && prefix.substring (openbracks + 1, closebracks).containsonly ("0123456789"))
            {
                number = prefix.substring (openbracks + 1, closebracks).getintvalue();
                prefix = prefix.substring (0, openbracks);
            }
        }

        // also use brackets if it ends in a digit.
        putnumbersinbrackets = putnumbersinbrackets
                                 || characterfunctions::isdigit (prefix.getlastcharacter());

        do
        {
            string newname (prefix);

            if (putnumbersinbrackets)
                newname << '(' << ++number << ')';
            else
                newname << ++number;

            f = getchildfile (newname + suffix);

        } while (f.exists());
    }

    return f;
}

file file::getnonexistentsibling (const bool putnumbersinbrackets) const
{
    if (! exists())
        return *this;

    return getparentdirectory().getnonexistentchildfile (getfilenamewithoutextension(),
                                                         getfileextension(),
                                                         putnumbersinbrackets);
}

//==============================================================================
string file::getfileextension() const
{
    const int indexofdot = fullpath.lastindexofchar ('.');

    if (indexofdot > fullpath.lastindexofchar (separator))
        return fullpath.substring (indexofdot);

    return string::empty;
}

bool file::hasfileextension (const string& possiblesuffix) const
{
    if (possiblesuffix.isempty())
        return fullpath.lastindexofchar ('.') <= fullpath.lastindexofchar (separator);

    const int semicolon = possiblesuffix.indexofchar (0, ';');

    if (semicolon >= 0)
    {
        return hasfileextension (possiblesuffix.substring (0, semicolon).trimend())
                || hasfileextension (possiblesuffix.substring (semicolon + 1).trimstart());
    }
    else
    {
        if (fullpath.endswithignorecase (possiblesuffix))
        {
            if (possiblesuffix.startswithchar ('.'))
                return true;

            const int dotpos = fullpath.length() - possiblesuffix.length() - 1;

            if (dotpos >= 0)
                return fullpath [dotpos] == '.';
        }
    }

    return false;
}

file file::withfileextension (const string& newextension) const
{
    if (fullpath.isempty())
        return file::nonexistent ();

    string filepart (getfilename());

    const int i = filepart.lastindexofchar ('.');
    if (i >= 0)
        filepart = filepart.substring (0, i);

    if (newextension.isnotempty() && ! newextension.startswithchar ('.'))
        filepart << '.';

    return getsiblingfile (filepart + newextension);
}

//==============================================================================
fileinputstream* file::createinputstream() const
{
    std::unique_ptr <fileinputstream> fin (new fileinputstream (*this));

    if (fin->openedok())
        return fin.release();

    return nullptr;
}

fileoutputstream* file::createoutputstream (const size_t buffersize) const
{
    std::unique_ptr <fileoutputstream> out (new fileoutputstream (*this, buffersize));

    return out->failedtoopen() ? nullptr
                               : out.release();
}

//==============================================================================
bool file::appenddata (const void* const datatoappend,
                       const size_t numberofbytes) const
{
    bassert (((std::ptrdiff_t) numberofbytes) >= 0);

    if (numberofbytes == 0)
        return true;

    fileoutputstream out (*this, 8192);
    return out.openedok() && out.write (datatoappend, numberofbytes);
}

bool file::appendtext (const string& text,
                       const bool asunicode,
                       const bool writeunicodeheaderbytes) const
{
    fileoutputstream out (*this);

    if (out.failedtoopen())
        return false;

    out.writetext (text, asunicode, writeunicodeheaderbytes);
    return true;
}

//==============================================================================
string file::createlegalpathname (const string& original)
{
    string s (original);
    string start;

    if (s[1] == ':')
    {
        start = s.substring (0, 2);
        s = s.substring (2);
    }

    return start + s.removecharacters ("\"#@,;:<>*^|?")
                    .substring (0, 1024);
}

string file::createlegalfilename (const string& original)
{
    string s (original.removecharacters ("\"#@,;:<>*^|?\\/"));

    const int maxlength = 128; // only the length of the filename, not the whole path
    const int len = s.length();

    if (len > maxlength)
    {
        const int lastdot = s.lastindexofchar ('.');

        if (lastdot > std::max (0, len - 12))
        {
            s = s.substring (0, maxlength - (len - lastdot))
                 + s.substring (lastdot);
        }
        else
        {
            s = s.substring (0, maxlength);
        }
    }

    return s;
}

//==============================================================================
static int countnumberofseparators (string::charpointertype s)
{
    int num = 0;

    for (;;)
    {
        const beast_wchar c = s.getandadvance();

        if (c == 0)
            break;

        if (c == file::separator)
            ++num;
    }

    return num;
}

string file::getrelativepathfrom (const file& dir)  const
{
    string thispath (fullpath);

    while (thispath.endswithchar (separator))
        thispath = thispath.droplastcharacters (1);

    string dirpath (addtrailingseparator (dir.existsasfile() ? dir.getparentdirectory().getfullpathname()
                                                             : dir.fullpath));

    int commonbitlength = 0;
    string::charpointertype thispathaftercommon (thispath.getcharpointer());
    string::charpointertype dirpathaftercommon  (dirpath.getcharpointer());

    {
        string::charpointertype thispathiter (thispath.getcharpointer());
        string::charpointertype dirpathiter  (dirpath.getcharpointer());

        for (int i = 0;;)
        {
            const beast_wchar c1 = thispathiter.getandadvance();
            const beast_wchar c2 = dirpathiter.getandadvance();

           #if names_are_case_sensitive
            if (c1 != c2
           #else
            if ((c1 != c2 && characterfunctions::tolowercase (c1) != characterfunctions::tolowercase (c2))
           #endif
                 || c1 == 0)
                break;

            ++i;

            if (c1 == separator)
            {
                thispathaftercommon = thispathiter;
                dirpathaftercommon  = dirpathiter;
                commonbitlength = i;
            }
        }
    }

    // if the only common bit is the root, then just return the full path..
    if (commonbitlength == 0 || (commonbitlength == 1 && thispath[1] == separator))
        return fullpath;

    const int numupdirectoriesneeded = countnumberofseparators (dirpathaftercommon);

    if (numupdirectoriesneeded == 0)
        return thispathaftercommon;

   #if beast_windows
    string s (string::repeatedstring ("..\\", numupdirectoriesneeded));
   #else
    string s (string::repeatedstring ("../",  numupdirectoriesneeded));
   #endif
    s.appendcharpointer (thispathaftercommon);
    return s;
}

//==============================================================================
file file::createtempfile (const string& filenameending)
{
    const file tempfile (getspeciallocation (tempdirectory)
                            .getchildfile ("temp_" + string::tohexstring (random::getsystemrandom().nextint()))
                            .withfileextension (filenameending));

    if (tempfile.exists())
        return createtempfile (filenameending);

    return tempfile;
}

//==============================================================================

class file_test : public unit_test::suite
{
public:
    template <class t1, class t2>
    bool
    expectequals (t1 const& t1, t2 const& t2)
    {
        return expect (t1 == t2);
    }

    void run()
    {
        testcase ("reading");

        const file home (file::getspeciallocation (file::userhomedirectory));
        const file temp (file::getspeciallocation (file::tempdirectory));

        expect (! file::nonexistent ().exists());
        expect (home.isdirectory());
        expect (home.exists());
        expect (! home.existsasfile());
        expect (file::getspeciallocation (file::userdocumentsdirectory).isdirectory());
        expect (file::getspeciallocation (file::userapplicationdatadirectory).isdirectory());
        expect (home.getvolumetotalsize() > 1024 * 1024);
        expect (home.getbytesfreeonvolume() > 0);
        expect (file::getcurrentworkingdirectory().exists());
        expect (home.setascurrentworkingdirectory());
       #if beast_windows
        expect (file::getcurrentworkingdirectory() == home);
       #endif

        testcase ("writing");

        file demofolder (temp.getchildfile ("beast unittests temp folder.folder"));
        expect (demofolder.deleterecursively());
        expect (demofolder.createdirectory());
        expect (demofolder.isdirectory());
        expect (demofolder.getparentdirectory() == temp);
        expect (temp.isdirectory());

        {
            array<file> files;
            temp.findchildfiles (files, file::findfilesanddirectories, false, "*");
            expect (files.contains (demofolder));
        }

        {
            array<file> files;
            temp.findchildfiles (files, file::finddirectories, true, "*.folder");
            expect (files.contains (demofolder));
        }

        file tempfile (demofolder.getnonexistentchildfile ("test", ".txt", false));

        expect (tempfile.getfileextension() == ".txt");
        expect (tempfile.hasfileextension (".txt"));
        expect (tempfile.hasfileextension ("txt"));
        expect (tempfile.withfileextension ("xyz").hasfileextension (".xyz"));
        expect (tempfile.withfileextension ("xyz").hasfileextension ("abc;xyz;foo"));
        expect (tempfile.withfileextension ("xyz").hasfileextension ("xyz;foo"));
        expect (! tempfile.withfileextension ("h").hasfileextension ("bar;foo;xx"));
        expect (tempfile.getsiblingfile ("foo").isachildof (temp));
        expect (tempfile.haswriteaccess());

        {
            fileoutputstream fo (tempfile);
            fo.write ("0123456789", 10);
        }

        expect (tempfile.exists());
        expect (tempfile.getsize() == 10);
        expect (std::abs ((int) (tempfile.getlastmodificationtime().tomilliseconds() - time::getcurrenttime().tomilliseconds())) < 3000);
        expect (! demofolder.containssubdirectories());

        expectequals (tempfile.getrelativepathfrom (demofolder.getparentdirectory()), demofolder.getfilename() + file::separatorstring + tempfile.getfilename());
        expectequals (demofolder.getparentdirectory().getrelativepathfrom (tempfile), ".." + file::separatorstring + ".." + file::separatorstring + demofolder.getparentdirectory().getfilename());

        expect (demofolder.getnumberofchildfiles (file::findfiles) == 1);
        expect (demofolder.getnumberofchildfiles (file::findfilesanddirectories) == 1);
        expect (demofolder.getnumberofchildfiles (file::finddirectories) == 0);
        demofolder.getnonexistentchildfile ("tempfolder", "", false).createdirectory();
        expect (demofolder.getnumberofchildfiles (file::finddirectories) == 1);
        expect (demofolder.getnumberofchildfiles (file::findfilesanddirectories) == 2);
        expect (demofolder.containssubdirectories());

        expect (tempfile.haswriteaccess());
        tempfile.setreadonly (true);
        expect (! tempfile.haswriteaccess());
        tempfile.setreadonly (false);
        expect (tempfile.haswriteaccess());

        time t (time::getcurrenttime());
        tempfile.setlastmodificationtime (t);
        time t2 = tempfile.getlastmodificationtime();
        expect (std::abs ((int) (t2.tomilliseconds() - t.tomilliseconds())) <= 1000);

        {
            expect (tempfile.getsize() == 10);
            fileoutputstream fo (tempfile);
            expect (fo.openedok());

            expect (fo.setposition  (7));
            expect (fo.truncate().wasok());
            expect (tempfile.getsize() == 7);
            fo.write ("789", 3);
            fo.flush();
            expect (tempfile.getsize() == 10);
        }

        testcase ("more writing");

        expect (tempfile.appenddata ("abcdefghij", 10));
        expect (tempfile.getsize() == 20);

        file tempfile2 (tempfile.getnonexistentsibling (false));
        expect (tempfile.copyfileto (tempfile2));
        expect (tempfile2.exists());
        expect (tempfile.deletefile());
        expect (! tempfile.exists());
        expect (tempfile2.movefileto (tempfile));
        expect (tempfile.exists());
        expect (! tempfile2.exists());

        expect (demofolder.deleterecursively());
        expect (! demofolder.exists());
    }
};

beast_define_testsuite_manual (file,beast_core,beast);

} // beast
