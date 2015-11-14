/*
** 2001 september 15
**
** the author disclaims copyright to this source code.  in place of
** a legal notice, here is a blessing:
**
**    may you do good and not evil.
**    may you find forgiveness for yourself and forgive others.
**    may you share freely, never taking more than you give.
**
*************************************************************************
** this header file defines the interface that the sqlite library
** presents to client programs.  if a c-function, structure, datatype,
** or constant definition does not appear in this file, then it is
** not a published api of sqlite, is subject to change without
** notice, and should not be referenced by programs that use sqlite.
**
** some of the definitions that are in this file are marked as
** "experimental".  experimental interfaces are normally new
** features recently added to sqlite.  we do not anticipate changes
** to experimental interfaces but reserve the right to make minor changes
** if experience from use "in the wild" suggest such changes are prudent.
**
** the official c-language api documentation for sqlite is derived
** from comments in this file.  this file is the authoritative source
** on how sqlite interfaces are suppose to operate.
**
** the name of this file under configuration management is "sqlite.h.in".
** the makefile makes some minor changes to this file (such as inserting
** the version number) and changes its name to "sqlite3.h" as
** part of the build process.
*/
#ifndef _sqlite3_h_
#define _sqlite3_h_
#include <stdarg.h>     /* needed for the definition of va_list */

/*
** make sure we can call this stuff from c++.
*/
#ifdef __cplusplus
extern "c" {
#endif


/*
** add the ability to override 'extern'
*/
#ifndef sqlite_extern
# define sqlite_extern extern
#endif

#ifndef sqlite_api
# define sqlite_api
#endif


/*
** these no-op macros are used in front of interfaces to mark those
** interfaces as either deprecated or experimental.  new applications
** should not use deprecated interfaces - they are support for backwards
** compatibility only.  application writers should be aware that
** experimental interfaces are subject to change in point releases.
**
** these macros used to resolve to various kinds of compiler magic that
** would generate warning messages when they were used.  but that
** compiler magic ended up generating such a flurry of bug reports
** that we have taken it all out and gone back to using simple
** noop macros.
*/
#define sqlite_deprecated
#define sqlite_experimental

/*
** ensure these symbols were not defined by some previous header file.
*/
#ifdef sqlite_version
# undef sqlite_version
#endif
#ifdef sqlite_version_number
# undef sqlite_version_number
#endif

/*
** capi3ref: compile-time library version numbers
**
** ^(the [sqlite_version] c preprocessor macro in the sqlite3.h header
** evaluates to a string literal that is the sqlite version in the
** format "x.y.z" where x is the major version number (always 3 for
** sqlite3) and y is the minor version number and z is the release number.)^
** ^(the [sqlite_version_number] c preprocessor macro resolves to an integer
** with the value (x*1000000 + y*1000 + z) where x, y, and z are the same
** numbers used in [sqlite_version].)^
** the sqlite_version_number for any given release of sqlite will also
** be larger than the release from which it is derived.  either y will
** be held constant and z will be incremented or else y will be incremented
** and z will be reset to zero.
**
** since version 3.6.18, sqlite source code has been stored in the
** <a href="http://www.fossil-scm.org/">fossil configuration management
** system</a>.  ^the sqlite_source_id macro evaluates to
** a string which identifies a particular check-in of sqlite
** within its configuration management system.  ^the sqlite_source_id
** string contains the date and time of the check-in (utc) and an sha1
** hash of the entire source tree.
**
** see also: [sqlite3_libversion()],
** [sqlite3_libversion_number()], [sqlite3_sourceid()],
** [sqlite_version()] and [sqlite_source_id()].
*/
#define sqlite_version        "3.8.7"
#define sqlite_version_number 3008007
#define sqlite_source_id      "2014-10-17 11:24:17 e4ab094f8afce0817f4074e823fabe59fc29ebb4"

/*
** capi3ref: run-time library version numbers
** keywords: sqlite3_version, sqlite3_sourceid
**
** these interfaces provide the same information as the [sqlite_version],
** [sqlite_version_number], and [sqlite_source_id] c preprocessor macros
** but are associated with the library instead of the header file.  ^(cautious
** programmers might include assert() statements in their application to
** verify that values returned by these interfaces match the macros in
** the header, and thus insure that the application is
** compiled with matching library and header files.
**
** <blockquote><pre>
** assert( sqlite3_libversion_number()==sqlite_version_number );
** assert( strcmp(sqlite3_sourceid(),sqlite_source_id)==0 );
** assert( strcmp(sqlite3_libversion(),sqlite_version)==0 );
** </pre></blockquote>)^
**
** ^the sqlite3_version[] string constant contains the text of [sqlite_version]
** macro.  ^the sqlite3_libversion() function returns a pointer to the
** to the sqlite3_version[] string constant.  the sqlite3_libversion()
** function is provided for use in dlls since dll users usually do not have
** direct access to string constants within the dll.  ^the
** sqlite3_libversion_number() function returns an integer equal to
** [sqlite_version_number].  ^the sqlite3_sourceid() function returns 
** a pointer to a string constant whose value is the same as the 
** [sqlite_source_id] c preprocessor macro.
**
** see also: [sqlite_version()] and [sqlite_source_id()].
*/
sqlite_api sqlite_extern const char sqlite3_version[];
sqlite_api const char *sqlite3_libversion(void);
sqlite_api const char *sqlite3_sourceid(void);
sqlite_api int sqlite3_libversion_number(void);

/*
** capi3ref: run-time library compilation options diagnostics
**
** ^the sqlite3_compileoption_used() function returns 0 or 1 
** indicating whether the specified option was defined at 
** compile time.  ^the sqlite_ prefix may be omitted from the 
** option name passed to sqlite3_compileoption_used().  
**
** ^the sqlite3_compileoption_get() function allows iterating
** over the list of options that were defined at compile time by
** returning the n-th compile time option string.  ^if n is out of range,
** sqlite3_compileoption_get() returns a null pointer.  ^the sqlite_ 
** prefix is omitted from any strings returned by 
** sqlite3_compileoption_get().
**
** ^support for the diagnostic functions sqlite3_compileoption_used()
** and sqlite3_compileoption_get() may be omitted by specifying the 
** [sqlite_omit_compileoption_diags] option at compile time.
**
** see also: sql functions [sqlite_compileoption_used()] and
** [sqlite_compileoption_get()] and the [compile_options pragma].
*/
#ifndef sqlite_omit_compileoption_diags
sqlite_api int sqlite3_compileoption_used(const char *zoptname);
sqlite_api const char *sqlite3_compileoption_get(int n);
#endif

/*
** capi3ref: test to see if the library is threadsafe
**
** ^the sqlite3_threadsafe() function returns zero if and only if
** sqlite was compiled with mutexing code omitted due to the
** [sqlite_threadsafe] compile-time option being set to 0.
**
** sqlite can be compiled with or without mutexes.  when
** the [sqlite_threadsafe] c preprocessor macro is 1 or 2, mutexes
** are enabled and sqlite is threadsafe.  when the
** [sqlite_threadsafe] macro is 0, 
** the mutexes are omitted.  without the mutexes, it is not safe
** to use sqlite concurrently from more than one thread.
**
** enabling mutexes incurs a measurable performance penalty.
** so if speed is of utmost importance, it makes sense to disable
** the mutexes.  but for maximum safety, mutexes should be enabled.
** ^the default behavior is for mutexes to be enabled.
**
** this interface can be used by an application to make sure that the
** version of sqlite that it is linking against was compiled with
** the desired setting of the [sqlite_threadsafe] macro.
**
** this interface only reports on the compile-time mutex setting
** of the [sqlite_threadsafe] flag.  if sqlite is compiled with
** sqlite_threadsafe=1 or =2 then mutexes are enabled by default but
** can be fully or partially disabled using a call to [sqlite3_config()]
** with the verbs [sqlite_config_singlethread], [sqlite_config_multithread],
** or [sqlite_config_mutex].  ^(the return value of the
** sqlite3_threadsafe() function shows only the compile-time setting of
** thread safety, not any run-time changes to that setting made by
** sqlite3_config(). in other words, the return value from sqlite3_threadsafe()
** is unchanged by calls to sqlite3_config().)^
**
** see the [threading mode] documentation for additional information.
*/
sqlite_api int sqlite3_threadsafe(void);

/*
** capi3ref: database connection handle
** keywords: {database connection} {database connections}
**
** each open sqlite database is represented by a pointer to an instance of
** the opaque structure named "sqlite3".  it is useful to think of an sqlite3
** pointer as an object.  the [sqlite3_open()], [sqlite3_open16()], and
** [sqlite3_open_v2()] interfaces are its constructors, and [sqlite3_close()]
** and [sqlite3_close_v2()] are its destructors.  there are many other
** interfaces (such as
** [sqlite3_prepare_v2()], [sqlite3_create_function()], and
** [sqlite3_busy_timeout()] to name but three) that are methods on an
** sqlite3 object.
*/
typedef struct sqlite3 sqlite3;

/*
** capi3ref: 64-bit integer types
** keywords: sqlite_int64 sqlite_uint64
**
** because there is no cross-platform way to specify 64-bit integer types
** sqlite includes typedefs for 64-bit signed and unsigned integers.
**
** the sqlite3_int64 and sqlite3_uint64 are the preferred type definitions.
** the sqlite_int64 and sqlite_uint64 types are supported for backwards
** compatibility only.
**
** ^the sqlite3_int64 and sqlite_int64 types can store integer values
** between -9223372036854775808 and +9223372036854775807 inclusive.  ^the
** sqlite3_uint64 and sqlite_uint64 types can store integer values 
** between 0 and +18446744073709551615 inclusive.
*/
#ifdef sqlite_int64_type
  typedef sqlite_int64_type sqlite_int64;
  typedef unsigned sqlite_int64_type sqlite_uint64;
#elif defined(_msc_ver) || defined(__borlandc__)
  typedef __int64 sqlite_int64;
  typedef unsigned __int64 sqlite_uint64;
#else
  typedef long long int sqlite_int64;
  typedef unsigned long long int sqlite_uint64;
#endif
typedef sqlite_int64 sqlite3_int64;
typedef sqlite_uint64 sqlite3_uint64;

/*
** if compiling for a processor that lacks floating point support,
** substitute integer for floating-point.
*/
#ifdef sqlite_omit_floating_point
# define double sqlite3_int64
#endif

/*
** capi3ref: closing a database connection
**
** ^the sqlite3_close() and sqlite3_close_v2() routines are destructors
** for the [sqlite3] object.
** ^calls to sqlite3_close() and sqlite3_close_v2() return [sqlite_ok] if
** the [sqlite3] object is successfully destroyed and all associated
** resources are deallocated.
**
** ^if the database connection is associated with unfinalized prepared
** statements or unfinished sqlite3_backup objects then sqlite3_close()
** will leave the database connection open and return [sqlite_busy].
** ^if sqlite3_close_v2() is called with unfinalized prepared statements
** and/or unfinished sqlite3_backups, then the database connection becomes
** an unusable "zombie" which will automatically be deallocated when the
** last prepared statement is finalized or the last sqlite3_backup is
** finished.  the sqlite3_close_v2() interface is intended for use with
** host languages that are garbage collected, and where the order in which
** destructors are called is arbitrary.
**
** applications should [sqlite3_finalize | finalize] all [prepared statements],
** [sqlite3_blob_close | close] all [blob handles], and 
** [sqlite3_backup_finish | finish] all [sqlite3_backup] objects associated
** with the [sqlite3] object prior to attempting to close the object.  ^if
** sqlite3_close_v2() is called on a [database connection] that still has
** outstanding [prepared statements], [blob handles], and/or
** [sqlite3_backup] objects then it returns [sqlite_ok] and the deallocation
** of resources is deferred until all [prepared statements], [blob handles],
** and [sqlite3_backup] objects are also destroyed.
**
** ^if an [sqlite3] object is destroyed while a transaction is open,
** the transaction is automatically rolled back.
**
** the c parameter to [sqlite3_close(c)] and [sqlite3_close_v2(c)]
** must be either a null
** pointer or an [sqlite3] object pointer obtained
** from [sqlite3_open()], [sqlite3_open16()], or
** [sqlite3_open_v2()], and not previously closed.
** ^calling sqlite3_close() or sqlite3_close_v2() with a null pointer
** argument is a harmless no-op.
*/
sqlite_api int sqlite3_close(sqlite3*);
sqlite_api int sqlite3_close_v2(sqlite3*);

/*
** the type for a callback function.
** this is legacy and deprecated.  it is included for historical
** compatibility and is not documented.
*/
typedef int (*sqlite3_callback)(void*,int,char**, char**);

/*
** capi3ref: one-step query execution interface
**
** the sqlite3_exec() interface is a convenience wrapper around
** [sqlite3_prepare_v2()], [sqlite3_step()], and [sqlite3_finalize()],
** that allows an application to run multiple statements of sql
** without having to use a lot of c code. 
**
** ^the sqlite3_exec() interface runs zero or more utf-8 encoded,
** semicolon-separate sql statements passed into its 2nd argument,
** in the context of the [database connection] passed in as its 1st
** argument.  ^if the callback function of the 3rd argument to
** sqlite3_exec() is not null, then it is invoked for each result row
** coming out of the evaluated sql statements.  ^the 4th argument to
** sqlite3_exec() is relayed through to the 1st argument of each
** callback invocation.  ^if the callback pointer to sqlite3_exec()
** is null, then no callback is ever invoked and result rows are
** ignored.
**
** ^if an error occurs while evaluating the sql statements passed into
** sqlite3_exec(), then execution of the current statement stops and
** subsequent statements are skipped.  ^if the 5th parameter to sqlite3_exec()
** is not null then any error message is written into memory obtained
** from [sqlite3_malloc()] and passed back through the 5th parameter.
** to avoid memory leaks, the application should invoke [sqlite3_free()]
** on error message strings returned through the 5th parameter of
** of sqlite3_exec() after the error message string is no longer needed.
** ^if the 5th parameter to sqlite3_exec() is not null and no errors
** occur, then sqlite3_exec() sets the pointer in its 5th parameter to
** null before returning.
**
** ^if an sqlite3_exec() callback returns non-zero, the sqlite3_exec()
** routine returns sqlite_abort without invoking the callback again and
** without running any subsequent sql statements.
**
** ^the 2nd argument to the sqlite3_exec() callback function is the
** number of columns in the result.  ^the 3rd argument to the sqlite3_exec()
** callback is an array of pointers to strings obtained as if from
** [sqlite3_column_text()], one for each column.  ^if an element of a
** result row is null then the corresponding string pointer for the
** sqlite3_exec() callback is a null pointer.  ^the 4th argument to the
** sqlite3_exec() callback is an array of pointers to strings where each
** entry represents the name of corresponding result column as obtained
** from [sqlite3_column_name()].
**
** ^if the 2nd parameter to sqlite3_exec() is a null pointer, a pointer
** to an empty string, or a pointer that contains only whitespace and/or 
** sql comments, then no sql statements are evaluated and the database
** is not changed.
**
** restrictions:
**
** <ul>
** <li> the application must insure that the 1st parameter to sqlite3_exec()
**      is a valid and open [database connection].
** <li> the application must not close the [database connection] specified by
**      the 1st parameter to sqlite3_exec() while sqlite3_exec() is running.
** <li> the application must not modify the sql statement text passed into
**      the 2nd parameter of sqlite3_exec() while sqlite3_exec() is running.
** </ul>
*/
sqlite_api int sqlite3_exec(
  sqlite3*,                                  /* an open database */
  const char *sql,                           /* sql to be evaluated */
  int (*callback)(void*,int,char**,char**),  /* callback function */
  void *,                                    /* 1st argument to callback */
  char **errmsg                              /* error msg written here */
);

/*
** capi3ref: result codes
** keywords: {result code definitions}
**
** many sqlite functions return an integer result code from the set shown
** here in order to indicate success or failure.
**
** new error codes may be added in future versions of sqlite.
**
** see also: [extended result code definitions]
*/
#define sqlite_ok           0   /* successful result */
/* beginning-of-error-codes */
#define sqlite_error        1   /* sql error or missing database */
#define sqlite_internal     2   /* internal logic error in sqlite */
#define sqlite_perm         3   /* access permission denied */
#define sqlite_abort        4   /* callback routine requested an abort */
#define sqlite_busy         5   /* the database file is locked */
#define sqlite_locked       6   /* a table in the database is locked */
#define sqlite_nomem        7   /* a malloc() failed */
#define sqlite_readonly     8   /* attempt to write a readonly database */
#define sqlite_interrupt    9   /* operation terminated by sqlite3_interrupt()*/
#define sqlite_ioerr       10   /* some kind of disk i/o error occurred */
#define sqlite_corrupt     11   /* the database disk image is malformed */
#define sqlite_notfound    12   /* unknown opcode in sqlite3_file_control() */
#define sqlite_full        13   /* insertion failed because database is full */
#define sqlite_cantopen    14   /* unable to open the database file */
#define sqlite_protocol    15   /* database lock protocol error */
#define sqlite_empty       16   /* database is empty */
#define sqlite_schema      17   /* the database schema changed */
#define sqlite_toobig      18   /* string or blob exceeds size limit */
#define sqlite_constraint  19   /* abort due to constraint violation */
#define sqlite_mismatch    20   /* data type mismatch */
#define sqlite_misuse      21   /* library used incorrectly */
#define sqlite_nolfs       22   /* uses os features not supported on host */
#define sqlite_auth        23   /* authorization denied */
#define sqlite_format      24   /* auxiliary database format error */
#define sqlite_range       25   /* 2nd parameter to sqlite3_bind out of range */
#define sqlite_notadb      26   /* file opened that is not a database file */
#define sqlite_notice      27   /* notifications from sqlite3_log() */
#define sqlite_warning     28   /* warnings from sqlite3_log() */
#define sqlite_row         100  /* sqlite3_step() has another row ready */
#define sqlite_done        101  /* sqlite3_step() has finished executing */
/* end-of-error-codes */

/*
** capi3ref: extended result codes
** keywords: {extended result code definitions}
**
** in its default configuration, sqlite api routines return one of 30 integer
** [result codes].  however, experience has shown that many of
** these result codes are too coarse-grained.  they do not provide as
** much information about problems as programmers might like.  in an effort to
** address this, newer versions of sqlite (version 3.3.8 and later) include
** support for additional result codes that provide more detailed information
** about errors. these [extended result codes] are enabled or disabled
** on a per database connection basis using the
** [sqlite3_extended_result_codes()] api.  or, the extended code for
** the most recent error can be obtained using
** [sqlite3_extended_errcode()].
*/
#define sqlite_ioerr_read              (sqlite_ioerr | (1<<8))
#define sqlite_ioerr_short_read        (sqlite_ioerr | (2<<8))
#define sqlite_ioerr_write             (sqlite_ioerr | (3<<8))
#define sqlite_ioerr_fsync             (sqlite_ioerr | (4<<8))
#define sqlite_ioerr_dir_fsync         (sqlite_ioerr | (5<<8))
#define sqlite_ioerr_truncate          (sqlite_ioerr | (6<<8))
#define sqlite_ioerr_fstat             (sqlite_ioerr | (7<<8))
#define sqlite_ioerr_unlock            (sqlite_ioerr | (8<<8))
#define sqlite_ioerr_rdlock            (sqlite_ioerr | (9<<8))
#define sqlite_ioerr_delete            (sqlite_ioerr | (10<<8))
#define sqlite_ioerr_blocked           (sqlite_ioerr | (11<<8))
#define sqlite_ioerr_nomem             (sqlite_ioerr | (12<<8))
#define sqlite_ioerr_access            (sqlite_ioerr | (13<<8))
#define sqlite_ioerr_checkreservedlock (sqlite_ioerr | (14<<8))
#define sqlite_ioerr_lock              (sqlite_ioerr | (15<<8))
#define sqlite_ioerr_close             (sqlite_ioerr | (16<<8))
#define sqlite_ioerr_dir_close         (sqlite_ioerr | (17<<8))
#define sqlite_ioerr_shmopen           (sqlite_ioerr | (18<<8))
#define sqlite_ioerr_shmsize           (sqlite_ioerr | (19<<8))
#define sqlite_ioerr_shmlock           (sqlite_ioerr | (20<<8))
#define sqlite_ioerr_shmmap            (sqlite_ioerr | (21<<8))
#define sqlite_ioerr_seek              (sqlite_ioerr | (22<<8))
#define sqlite_ioerr_delete_noent      (sqlite_ioerr | (23<<8))
#define sqlite_ioerr_mmap              (sqlite_ioerr | (24<<8))
#define sqlite_ioerr_gettemppath       (sqlite_ioerr | (25<<8))
#define sqlite_ioerr_convpath          (sqlite_ioerr | (26<<8))
#define sqlite_locked_sharedcache      (sqlite_locked |  (1<<8))
#define sqlite_busy_recovery           (sqlite_busy   |  (1<<8))
#define sqlite_busy_snapshot           (sqlite_busy   |  (2<<8))
#define sqlite_cantopen_notempdir      (sqlite_cantopen | (1<<8))
#define sqlite_cantopen_isdir          (sqlite_cantopen | (2<<8))
#define sqlite_cantopen_fullpath       (sqlite_cantopen | (3<<8))
#define sqlite_cantopen_convpath       (sqlite_cantopen | (4<<8))
#define sqlite_corrupt_vtab            (sqlite_corrupt | (1<<8))
#define sqlite_readonly_recovery       (sqlite_readonly | (1<<8))
#define sqlite_readonly_cantlock       (sqlite_readonly | (2<<8))
#define sqlite_readonly_rollback       (sqlite_readonly | (3<<8))
#define sqlite_readonly_dbmoved        (sqlite_readonly | (4<<8))
#define sqlite_abort_rollback          (sqlite_abort | (2<<8))
#define sqlite_constraint_check        (sqlite_constraint | (1<<8))
#define sqlite_constraint_commithook   (sqlite_constraint | (2<<8))
#define sqlite_constraint_foreignkey   (sqlite_constraint | (3<<8))
#define sqlite_constraint_function     (sqlite_constraint | (4<<8))
#define sqlite_constraint_notnull      (sqlite_constraint | (5<<8))
#define sqlite_constraint_primarykey   (sqlite_constraint | (6<<8))
#define sqlite_constraint_trigger      (sqlite_constraint | (7<<8))
#define sqlite_constraint_unique       (sqlite_constraint | (8<<8))
#define sqlite_constraint_vtab         (sqlite_constraint | (9<<8))
#define sqlite_constraint_rowid        (sqlite_constraint |(10<<8))
#define sqlite_notice_recover_wal      (sqlite_notice | (1<<8))
#define sqlite_notice_recover_rollback (sqlite_notice | (2<<8))
#define sqlite_warning_autoindex       (sqlite_warning | (1<<8))
#define sqlite_auth_user               (sqlite_auth | (1<<8))

/*
** capi3ref: flags for file open operations
**
** these bit values are intended for use in the
** 3rd parameter to the [sqlite3_open_v2()] interface and
** in the 4th parameter to the [sqlite3_vfs.xopen] method.
*/
#define sqlite_open_readonly         0x00000001  /* ok for sqlite3_open_v2() */
#define sqlite_open_readwrite        0x00000002  /* ok for sqlite3_open_v2() */
#define sqlite_open_create           0x00000004  /* ok for sqlite3_open_v2() */
#define sqlite_open_deleteonclose    0x00000008  /* vfs only */
#define sqlite_open_exclusive        0x00000010  /* vfs only */
#define sqlite_open_autoproxy        0x00000020  /* vfs only */
#define sqlite_open_uri              0x00000040  /* ok for sqlite3_open_v2() */
#define sqlite_open_memory           0x00000080  /* ok for sqlite3_open_v2() */
#define sqlite_open_main_db          0x00000100  /* vfs only */
#define sqlite_open_temp_db          0x00000200  /* vfs only */
#define sqlite_open_transient_db     0x00000400  /* vfs only */
#define sqlite_open_main_journal     0x00000800  /* vfs only */
#define sqlite_open_temp_journal     0x00001000  /* vfs only */
#define sqlite_open_subjournal       0x00002000  /* vfs only */
#define sqlite_open_master_journal   0x00004000  /* vfs only */
#define sqlite_open_nomutex          0x00008000  /* ok for sqlite3_open_v2() */
#define sqlite_open_fullmutex        0x00010000  /* ok for sqlite3_open_v2() */
#define sqlite_open_sharedcache      0x00020000  /* ok for sqlite3_open_v2() */
#define sqlite_open_privatecache     0x00040000  /* ok for sqlite3_open_v2() */
#define sqlite_open_wal              0x00080000  /* vfs only */

/* reserved:                         0x00f00000 */

/*
** capi3ref: device characteristics
**
** the xdevicecharacteristics method of the [sqlite3_io_methods]
** object returns an integer which is a vector of these
** bit values expressing i/o characteristics of the mass storage
** device that holds the file that the [sqlite3_io_methods]
** refers to.
**
** the sqlite_iocap_atomic property means that all writes of
** any size are atomic.  the sqlite_iocap_atomicnnn values
** mean that writes of blocks that are nnn bytes in size and
** are aligned to an address which is an integer multiple of
** nnn are atomic.  the sqlite_iocap_safe_append value means
** that when data is appended to a file, the data is appended
** first then the size of the file is extended, never the other
** way around.  the sqlite_iocap_sequential property means that
** information is written to disk in the same order as calls
** to xwrite().  the sqlite_iocap_powersafe_overwrite property means that
** after reboot following a crash or power loss, the only bytes in a
** file that were written at the application level might have changed
** and that adjacent bytes, even bytes within the same sector are
** guaranteed to be unchanged.  the sqlite_iocap_undeletable_when_open
** flag indicate that a file cannot be deleted when open.  the
** sqlite_iocap_immutable flag indicates that the file is on
** read-only media and cannot be changed even by processes with
** elevated privileges.
*/
#define sqlite_iocap_atomic                 0x00000001
#define sqlite_iocap_atomic512              0x00000002
#define sqlite_iocap_atomic1k               0x00000004
#define sqlite_iocap_atomic2k               0x00000008
#define sqlite_iocap_atomic4k               0x00000010
#define sqlite_iocap_atomic8k               0x00000020
#define sqlite_iocap_atomic16k              0x00000040
#define sqlite_iocap_atomic32k              0x00000080
#define sqlite_iocap_atomic64k              0x00000100
#define sqlite_iocap_safe_append            0x00000200
#define sqlite_iocap_sequential             0x00000400
#define sqlite_iocap_undeletable_when_open  0x00000800
#define sqlite_iocap_powersafe_overwrite    0x00001000
#define sqlite_iocap_immutable              0x00002000

/*
** capi3ref: file locking levels
**
** sqlite uses one of these integer values as the second
** argument to calls it makes to the xlock() and xunlock() methods
** of an [sqlite3_io_methods] object.
*/
#define sqlite_lock_none          0
#define sqlite_lock_shared        1
#define sqlite_lock_reserved      2
#define sqlite_lock_pending       3
#define sqlite_lock_exclusive     4

/*
** capi3ref: synchronization type flags
**
** when sqlite invokes the xsync() method of an
** [sqlite3_io_methods] object it uses a combination of
** these integer values as the second argument.
**
** when the sqlite_sync_dataonly flag is used, it means that the
** sync operation only needs to flush data to mass storage.  inode
** information need not be flushed. if the lower four bits of the flag
** equal sqlite_sync_normal, that means to use normal fsync() semantics.
** if the lower four bits equal sqlite_sync_full, that means
** to use mac os x style fullsync instead of fsync().
**
** do not confuse the sqlite_sync_normal and sqlite_sync_full flags
** with the [pragma synchronous]=normal and [pragma synchronous]=full
** settings.  the [synchronous pragma] determines when calls to the
** xsync vfs method occur and applies uniformly across all platforms.
** the sqlite_sync_normal and sqlite_sync_full flags determine how
** energetic or rigorous or forceful the sync operations are and
** only make a difference on mac osx for the default sqlite code.
** (third-party vfs implementations might also make the distinction
** between sqlite_sync_normal and sqlite_sync_full, but among the
** operating systems natively supported by sqlite, only mac osx
** cares about the difference.)
*/
#define sqlite_sync_normal        0x00002
#define sqlite_sync_full          0x00003
#define sqlite_sync_dataonly      0x00010

/*
** capi3ref: os interface open file handle
**
** an [sqlite3_file] object represents an open file in the 
** [sqlite3_vfs | os interface layer].  individual os interface
** implementations will
** want to subclass this object by appending additional fields
** for their own use.  the pmethods entry is a pointer to an
** [sqlite3_io_methods] object that defines methods for performing
** i/o operations on the open file.
*/
typedef struct sqlite3_file sqlite3_file;
struct sqlite3_file {
  const struct sqlite3_io_methods *pmethods;  /* methods for an open file */
};

/*
** capi3ref: os interface file virtual methods object
**
** every file opened by the [sqlite3_vfs.xopen] method populates an
** [sqlite3_file] object (or, more commonly, a subclass of the
** [sqlite3_file] object) with a pointer to an instance of this object.
** this object defines the methods used to perform various operations
** against the open file represented by the [sqlite3_file] object.
**
** if the [sqlite3_vfs.xopen] method sets the sqlite3_file.pmethods element 
** to a non-null pointer, then the sqlite3_io_methods.xclose method
** may be invoked even if the [sqlite3_vfs.xopen] reported that it failed.  the
** only way to prevent a call to xclose following a failed [sqlite3_vfs.xopen]
** is for the [sqlite3_vfs.xopen] to set the sqlite3_file.pmethods element
** to null.
**
** the flags argument to xsync may be one of [sqlite_sync_normal] or
** [sqlite_sync_full].  the first choice is the normal fsync().
** the second choice is a mac os x style fullsync.  the [sqlite_sync_dataonly]
** flag may be ored in to indicate that only the data of the file
** and not its inode needs to be synced.
**
** the integer values to xlock() and xunlock() are one of
** <ul>
** <li> [sqlite_lock_none],
** <li> [sqlite_lock_shared],
** <li> [sqlite_lock_reserved],
** <li> [sqlite_lock_pending], or
** <li> [sqlite_lock_exclusive].
** </ul>
** xlock() increases the lock. xunlock() decreases the lock.
** the xcheckreservedlock() method checks whether any database connection,
** either in this process or in some other process, is holding a reserved,
** pending, or exclusive lock on the file.  it returns true
** if such a lock exists and false otherwise.
**
** the xfilecontrol() method is a generic interface that allows custom
** vfs implementations to directly control an open file using the
** [sqlite3_file_control()] interface.  the second "op" argument is an
** integer opcode.  the third argument is a generic pointer intended to
** point to a structure that may contain arguments or space in which to
** write return values.  potential uses for xfilecontrol() might be
** functions to enable blocking locks with timeouts, to change the
** locking strategy (for example to use dot-file locks), to inquire
** about the status of a lock, or to break stale locks.  the sqlite
** core reserves all opcodes less than 100 for its own use.
** a [file control opcodes | list of opcodes] less than 100 is available.
** applications that define a custom xfilecontrol method should use opcodes
** greater than 100 to avoid conflicts.  vfs implementations should
** return [sqlite_notfound] for file control opcodes that they do not
** recognize.
**
** the xsectorsize() method returns the sector size of the
** device that underlies the file.  the sector size is the
** minimum write that can be performed without disturbing
** other bytes in the file.  the xdevicecharacteristics()
** method returns a bit vector describing behaviors of the
** underlying device:
**
** <ul>
** <li> [sqlite_iocap_atomic]
** <li> [sqlite_iocap_atomic512]
** <li> [sqlite_iocap_atomic1k]
** <li> [sqlite_iocap_atomic2k]
** <li> [sqlite_iocap_atomic4k]
** <li> [sqlite_iocap_atomic8k]
** <li> [sqlite_iocap_atomic16k]
** <li> [sqlite_iocap_atomic32k]
** <li> [sqlite_iocap_atomic64k]
** <li> [sqlite_iocap_safe_append]
** <li> [sqlite_iocap_sequential]
** </ul>
**
** the sqlite_iocap_atomic property means that all writes of
** any size are atomic.  the sqlite_iocap_atomicnnn values
** mean that writes of blocks that are nnn bytes in size and
** are aligned to an address which is an integer multiple of
** nnn are atomic.  the sqlite_iocap_safe_append value means
** that when data is appended to a file, the data is appended
** first then the size of the file is extended, never the other
** way around.  the sqlite_iocap_sequential property means that
** information is written to disk in the same order as calls
** to xwrite().
**
** if xread() returns sqlite_ioerr_short_read it must also fill
** in the unread portions of the buffer with zeros.  a vfs that
** fails to zero-fill short reads might seem to work.  however,
** failure to zero-fill short reads will eventually lead to
** database corruption.
*/
typedef struct sqlite3_io_methods sqlite3_io_methods;
struct sqlite3_io_methods {
  int iversion;
  int (*xclose)(sqlite3_file*);
  int (*xread)(sqlite3_file*, void*, int iamt, sqlite3_int64 iofst);
  int (*xwrite)(sqlite3_file*, const void*, int iamt, sqlite3_int64 iofst);
  int (*xtruncate)(sqlite3_file*, sqlite3_int64 size);
  int (*xsync)(sqlite3_file*, int flags);
  int (*xfilesize)(sqlite3_file*, sqlite3_int64 *psize);
  int (*xlock)(sqlite3_file*, int);
  int (*xunlock)(sqlite3_file*, int);
  int (*xcheckreservedlock)(sqlite3_file*, int *presout);
  int (*xfilecontrol)(sqlite3_file*, int op, void *parg);
  int (*xsectorsize)(sqlite3_file*);
  int (*xdevicecharacteristics)(sqlite3_file*);
  /* methods above are valid for version 1 */
  int (*xshmmap)(sqlite3_file*, int ipg, int pgsz, int, void volatile**);
  int (*xshmlock)(sqlite3_file*, int offset, int n, int flags);
  void (*xshmbarrier)(sqlite3_file*);
  int (*xshmunmap)(sqlite3_file*, int deleteflag);
  /* methods above are valid for version 2 */
  int (*xfetch)(sqlite3_file*, sqlite3_int64 iofst, int iamt, void **pp);
  int (*xunfetch)(sqlite3_file*, sqlite3_int64 iofst, void *p);
  /* methods above are valid for version 3 */
  /* additional methods may be added in future releases */
};

/*
** capi3ref: standard file control opcodes
** keywords: {file control opcodes} {file control opcode}
**
** these integer constants are opcodes for the xfilecontrol method
** of the [sqlite3_io_methods] object and for the [sqlite3_file_control()]
** interface.
**
** the [sqlite_fcntl_lockstate] opcode is used for debugging.  this
** opcode causes the xfilecontrol method to write the current state of
** the lock (one of [sqlite_lock_none], [sqlite_lock_shared],
** [sqlite_lock_reserved], [sqlite_lock_pending], or [sqlite_lock_exclusive])
** into an integer that the parg argument points to. this capability
** is used during testing and only needs to be supported when sqlite_test
** is defined.
** <ul>
** <li>[[sqlite_fcntl_size_hint]]
** the [sqlite_fcntl_size_hint] opcode is used by sqlite to give the vfs
** layer a hint of how large the database file will grow to be during the
** current transaction.  this hint is not guaranteed to be accurate but it
** is often close.  the underlying vfs might choose to preallocate database
** file space based on this hint in order to help writes to the database
** file run faster.
**
** <li>[[sqlite_fcntl_chunk_size]]
** the [sqlite_fcntl_chunk_size] opcode is used to request that the vfs
** extends and truncates the database file in chunks of a size specified
** by the user. the fourth argument to [sqlite3_file_control()] should 
** point to an integer (type int) containing the new chunk-size to use
** for the nominated database. allocating database file space in large
** chunks (say 1mb at a time), may reduce file-system fragmentation and
** improve performance on some systems.
**
** <li>[[sqlite_fcntl_file_pointer]]
** the [sqlite_fcntl_file_pointer] opcode is used to obtain a pointer
** to the [sqlite3_file] object associated with a particular database
** connection.  see the [sqlite3_file_control()] documentation for
** additional information.
**
** <li>[[sqlite_fcntl_sync_omitted]]
** no longer in use.
**
** <li>[[sqlite_fcntl_sync]]
** the [sqlite_fcntl_sync] opcode is generated internally by sqlite and
** sent to the vfs immediately before the xsync method is invoked on a
** database file descriptor. or, if the xsync method is not invoked 
** because the user has configured sqlite with 
** [pragma synchronous | pragma synchronous=off] it is invoked in place 
** of the xsync method. in most cases, the pointer argument passed with
** this file-control is null. however, if the database file is being synced
** as part of a multi-database commit, the argument points to a nul-terminated
** string containing the transactions master-journal file name. vfses that 
** do not need this signal should silently ignore this opcode. applications 
** should not call [sqlite3_file_control()] with this opcode as doing so may 
** disrupt the operation of the specialized vfses that do require it.  
**
** <li>[[sqlite_fcntl_commit_phasetwo]]
** the [sqlite_fcntl_commit_phasetwo] opcode is generated internally by sqlite
** and sent to the vfs after a transaction has been committed immediately
** but before the database is unlocked. vfses that do not need this signal
** should silently ignore this opcode. applications should not call
** [sqlite3_file_control()] with this opcode as doing so may disrupt the 
** operation of the specialized vfses that do require it.  
**
** <li>[[sqlite_fcntl_win32_av_retry]]
** ^the [sqlite_fcntl_win32_av_retry] opcode is used to configure automatic
** retry counts and intervals for certain disk i/o operations for the
** windows [vfs] in order to provide robustness in the presence of
** anti-virus programs.  by default, the windows vfs will retry file read,
** file write, and file delete operations up to 10 times, with a delay
** of 25 milliseconds before the first retry and with the delay increasing
** by an additional 25 milliseconds with each subsequent retry.  this
** opcode allows these two values (10 retries and 25 milliseconds of delay)
** to be adjusted.  the values are changed for all database connections
** within the same process.  the argument is a pointer to an array of two
** integers where the first integer i the new retry count and the second
** integer is the delay.  if either integer is negative, then the setting
** is not changed but instead the prior value of that setting is written
** into the array entry, allowing the current retry settings to be
** interrogated.  the zdbname parameter is ignored.
**
** <li>[[sqlite_fcntl_persist_wal]]
** ^the [sqlite_fcntl_persist_wal] opcode is used to set or query the
** persistent [wal | write ahead log] setting.  by default, the auxiliary
** write ahead log and shared memory files used for transaction control
** are automatically deleted when the latest connection to the database
** closes.  setting persistent wal mode causes those files to persist after
** close.  persisting the files is useful when other processes that do not
** have write permission on the directory containing the database file want
** to read the database file, as the wal and shared memory files must exist
** in order for the database to be readable.  the fourth parameter to
** [sqlite3_file_control()] for this opcode should be a pointer to an integer.
** that integer is 0 to disable persistent wal mode or 1 to enable persistent
** wal mode.  if the integer is -1, then it is overwritten with the current
** wal persistence setting.
**
** <li>[[sqlite_fcntl_powersafe_overwrite]]
** ^the [sqlite_fcntl_powersafe_overwrite] opcode is used to set or query the
** persistent "powersafe-overwrite" or "psow" setting.  the psow setting
** determines the [sqlite_iocap_powersafe_overwrite] bit of the
** xdevicecharacteristics methods. the fourth parameter to
** [sqlite3_file_control()] for this opcode should be a pointer to an integer.
** that integer is 0 to disable zero-damage mode or 1 to enable zero-damage
** mode.  if the integer is -1, then it is overwritten with the current
** zero-damage mode setting.
**
** <li>[[sqlite_fcntl_overwrite]]
** ^the [sqlite_fcntl_overwrite] opcode is invoked by sqlite after opening
** a write transaction to indicate that, unless it is rolled back for some
** reason, the entire database file will be overwritten by the current 
** transaction. this is used by vacuum operations.
**
** <li>[[sqlite_fcntl_vfsname]]
** ^the [sqlite_fcntl_vfsname] opcode can be used to obtain the names of
** all [vfses] in the vfs stack.  the names are of all vfs shims and the
** final bottom-level vfs are written into memory obtained from 
** [sqlite3_malloc()] and the result is stored in the char* variable
** that the fourth parameter of [sqlite3_file_control()] points to.
** the caller is responsible for freeing the memory when done.  as with
** all file-control actions, there is no guarantee that this will actually
** do anything.  callers should initialize the char* variable to a null
** pointer in case this file-control is not implemented.  this file-control
** is intended for diagnostic use only.
**
** <li>[[sqlite_fcntl_pragma]]
** ^whenever a [pragma] statement is parsed, an [sqlite_fcntl_pragma] 
** file control is sent to the open [sqlite3_file] object corresponding
** to the database file to which the pragma statement refers. ^the argument
** to the [sqlite_fcntl_pragma] file control is an array of
** pointers to strings (char**) in which the second element of the array
** is the name of the pragma and the third element is the argument to the
** pragma or null if the pragma has no argument.  ^the handler for an
** [sqlite_fcntl_pragma] file control can optionally make the first element
** of the char** argument point to a string obtained from [sqlite3_mprintf()]
** or the equivalent and that string will become the result of the pragma or
** the error message if the pragma fails. ^if the
** [sqlite_fcntl_pragma] file control returns [sqlite_notfound], then normal 
** [pragma] processing continues.  ^if the [sqlite_fcntl_pragma]
** file control returns [sqlite_ok], then the parser assumes that the
** vfs has handled the pragma itself and the parser generates a no-op
** prepared statement.  ^if the [sqlite_fcntl_pragma] file control returns
** any result code other than [sqlite_ok] or [sqlite_notfound], that means
** that the vfs encountered an error while handling the [pragma] and the
** compilation of the pragma fails with an error.  ^the [sqlite_fcntl_pragma]
** file control occurs at the beginning of pragma statement analysis and so
** it is able to override built-in [pragma] statements.
**
** <li>[[sqlite_fcntl_busyhandler]]
** ^the [sqlite_fcntl_busyhandler]
** file-control may be invoked by sqlite on the database file handle
** shortly after it is opened in order to provide a custom vfs with access
** to the connections busy-handler callback. the argument is of type (void **)
** - an array of two (void *) values. the first (void *) actually points
** to a function of type (int (*)(void *)). in order to invoke the connections
** busy-handler, this function should be invoked with the second (void *) in
** the array as the only argument. if it returns non-zero, then the operation
** should be retried. if it returns zero, the custom vfs should abandon the
** current operation.
**
** <li>[[sqlite_fcntl_tempfilename]]
** ^application can invoke the [sqlite_fcntl_tempfilename] file-control
** to have sqlite generate a
** temporary filename using the same algorithm that is followed to generate
** temporary filenames for temp tables and other internal uses.  the
** argument should be a char** which will be filled with the filename
** written into memory obtained from [sqlite3_malloc()].  the caller should
** invoke [sqlite3_free()] on the result to avoid a memory leak.
**
** <li>[[sqlite_fcntl_mmap_size]]
** the [sqlite_fcntl_mmap_size] file control is used to query or set the
** maximum number of bytes that will be used for memory-mapped i/o.
** the argument is a pointer to a value of type sqlite3_int64 that
** is an advisory maximum number of bytes in the file to memory map.  the
** pointer is overwritten with the old value.  the limit is not changed if
** the value originally pointed to is negative, and so the current limit 
** can be queried by passing in a pointer to a negative number.  this
** file-control is used internally to implement [pragma mmap_size].
**
** <li>[[sqlite_fcntl_trace]]
** the [sqlite_fcntl_trace] file control provides advisory information
** to the vfs about what the higher layers of the sqlite stack are doing.
** this file control is used by some vfs activity tracing [shims].
** the argument is a zero-terminated string.  higher layers in the
** sqlite stack may generate instances of this file control if
** the [sqlite_use_fcntl_trace] compile-time option is enabled.
**
** <li>[[sqlite_fcntl_has_moved]]
** the [sqlite_fcntl_has_moved] file control interprets its argument as a
** pointer to an integer and it writes a boolean into that integer depending
** on whether or not the file has been renamed, moved, or deleted since it
** was first opened.
**
** <li>[[sqlite_fcntl_win32_set_handle]]
** the [sqlite_fcntl_win32_set_handle] opcode is used for debugging.  this
** opcode causes the xfilecontrol method to swap the file handle with the one
** pointed to by the parg argument.  this capability is used during testing
** and only needs to be supported when sqlite_test is defined.
**
** </ul>
*/
#define sqlite_fcntl_lockstate               1
#define sqlite_get_lockproxyfile             2
#define sqlite_set_lockproxyfile             3
#define sqlite_last_errno                    4
#define sqlite_fcntl_size_hint               5
#define sqlite_fcntl_chunk_size              6
#define sqlite_fcntl_file_pointer            7
#define sqlite_fcntl_sync_omitted            8
#define sqlite_fcntl_win32_av_retry          9
#define sqlite_fcntl_persist_wal            10
#define sqlite_fcntl_overwrite              11
#define sqlite_fcntl_vfsname                12
#define sqlite_fcntl_powersafe_overwrite    13
#define sqlite_fcntl_pragma                 14
#define sqlite_fcntl_busyhandler            15
#define sqlite_fcntl_tempfilename           16
#define sqlite_fcntl_mmap_size              18
#define sqlite_fcntl_trace                  19
#define sqlite_fcntl_has_moved              20
#define sqlite_fcntl_sync                   21
#define sqlite_fcntl_commit_phasetwo        22
#define sqlite_fcntl_win32_set_handle       23

/*
** capi3ref: mutex handle
**
** the mutex module within sqlite defines [sqlite3_mutex] to be an
** abstract type for a mutex object.  the sqlite core never looks
** at the internal representation of an [sqlite3_mutex].  it only
** deals with pointers to the [sqlite3_mutex] object.
**
** mutexes are created using [sqlite3_mutex_alloc()].
*/
typedef struct sqlite3_mutex sqlite3_mutex;

/*
** capi3ref: os interface object
**
** an instance of the sqlite3_vfs object defines the interface between
** the sqlite core and the underlying operating system.  the "vfs"
** in the name of the object stands for "virtual file system".  see
** the [vfs | vfs documentation] for further information.
**
** the value of the iversion field is initially 1 but may be larger in
** future versions of sqlite.  additional fields may be appended to this
** object when the iversion value is increased.  note that the structure
** of the sqlite3_vfs object changes in the transaction between
** sqlite version 3.5.9 and 3.6.0 and yet the iversion field was not
** modified.
**
** the szosfile field is the size of the subclassed [sqlite3_file]
** structure used by this vfs.  mxpathname is the maximum length of
** a pathname in this vfs.
**
** registered sqlite3_vfs objects are kept on a linked list formed by
** the pnext pointer.  the [sqlite3_vfs_register()]
** and [sqlite3_vfs_unregister()] interfaces manage this list
** in a thread-safe way.  the [sqlite3_vfs_find()] interface
** searches the list.  neither the application code nor the vfs
** implementation should use the pnext pointer.
**
** the pnext field is the only field in the sqlite3_vfs
** structure that sqlite will ever modify.  sqlite will only access
** or modify this field while holding a particular static mutex.
** the application should never modify anything within the sqlite3_vfs
** object once the object has been registered.
**
** the zname field holds the name of the vfs module.  the name must
** be unique across all vfs modules.
**
** [[sqlite3_vfs.xopen]]
** ^sqlite guarantees that the zfilename parameter to xopen
** is either a null pointer or string obtained
** from xfullpathname() with an optional suffix added.
** ^if a suffix is added to the zfilename parameter, it will
** consist of a single "-" character followed by no more than
** 11 alphanumeric and/or "-" characters.
** ^sqlite further guarantees that
** the string will be valid and unchanged until xclose() is
** called. because of the previous sentence,
** the [sqlite3_file] can safely store a pointer to the
** filename if it needs to remember the filename for some reason.
** if the zfilename parameter to xopen is a null pointer then xopen
** must invent its own temporary name for the file.  ^whenever the 
** xfilename parameter is null it will also be the case that the
** flags parameter will include [sqlite_open_deleteonclose].
**
** the flags argument to xopen() includes all bits set in
** the flags argument to [sqlite3_open_v2()].  or if [sqlite3_open()]
** or [sqlite3_open16()] is used, then flags includes at least
** [sqlite_open_readwrite] | [sqlite_open_create]. 
** if xopen() opens a file read-only then it sets *poutflags to
** include [sqlite_open_readonly].  other bits in *poutflags may be set.
**
** ^(sqlite will also add one of the following flags to the xopen()
** call, depending on the object being opened:
**
** <ul>
** <li>  [sqlite_open_main_db]
** <li>  [sqlite_open_main_journal]
** <li>  [sqlite_open_temp_db]
** <li>  [sqlite_open_temp_journal]
** <li>  [sqlite_open_transient_db]
** <li>  [sqlite_open_subjournal]
** <li>  [sqlite_open_master_journal]
** <li>  [sqlite_open_wal]
** </ul>)^
**
** the file i/o implementation can use the object type flags to
** change the way it deals with files.  for example, an application
** that does not care about crash recovery or rollback might make
** the open of a journal file a no-op.  writes to this journal would
** also be no-ops, and any attempt to read the journal would return
** sqlite_ioerr.  or the implementation might recognize that a database
** file will be doing page-aligned sector reads and writes in a random
** order and set up its i/o subsystem accordingly.
**
** sqlite might also add one of the following flags to the xopen method:
**
** <ul>
** <li> [sqlite_open_deleteonclose]
** <li> [sqlite_open_exclusive]
** </ul>
**
** the [sqlite_open_deleteonclose] flag means the file should be
** deleted when it is closed.  ^the [sqlite_open_deleteonclose]
** will be set for temp databases and their journals, transient
** databases, and subjournals.
**
** ^the [sqlite_open_exclusive] flag is always used in conjunction
** with the [sqlite_open_create] flag, which are both directly
** analogous to the o_excl and o_creat flags of the posix open()
** api.  the sqlite_open_exclusive flag, when paired with the 
** sqlite_open_create, is used to indicate that file should always
** be created, and that it is an error if it already exists.
** it is <i>not</i> used to indicate the file should be opened 
** for exclusive access.
**
** ^at least szosfile bytes of memory are allocated by sqlite
** to hold the  [sqlite3_file] structure passed as the third
** argument to xopen.  the xopen method does not have to
** allocate the structure; it should just fill it in.  note that
** the xopen method must set the sqlite3_file.pmethods to either
** a valid [sqlite3_io_methods] object or to null.  xopen must do
** this even if the open fails.  sqlite expects that the sqlite3_file.pmethods
** element will be valid after xopen returns regardless of the success
** or failure of the xopen call.
**
** [[sqlite3_vfs.xaccess]]
** ^the flags argument to xaccess() may be [sqlite_access_exists]
** to test for the existence of a file, or [sqlite_access_readwrite] to
** test whether a file is readable and writable, or [sqlite_access_read]
** to test whether a file is at least readable.   the file can be a
** directory.
**
** ^sqlite will always allocate at least mxpathname+1 bytes for the
** output buffer xfullpathname.  the exact size of the output buffer
** is also passed as a parameter to both  methods. if the output buffer
** is not large enough, [sqlite_cantopen] should be returned. since this is
** handled as a fatal error by sqlite, vfs implementations should endeavor
** to prevent this by setting mxpathname to a sufficiently large value.
**
** the xrandomness(), xsleep(), xcurrenttime(), and xcurrenttimeint64()
** interfaces are not strictly a part of the filesystem, but they are
** included in the vfs structure for completeness.
** the xrandomness() function attempts to return nbytes bytes
** of good-quality randomness into zout.  the return value is
** the actual number of bytes of randomness obtained.
** the xsleep() method causes the calling thread to sleep for at
** least the number of microseconds given.  ^the xcurrenttime()
** method returns a julian day number for the current date and time as
** a floating point value.
** ^the xcurrenttimeint64() method returns, as an integer, the julian
** day number multiplied by 86400000 (the number of milliseconds in 
** a 24-hour day).  
** ^sqlite will use the xcurrenttimeint64() method to get the current
** date and time if that method is available (if iversion is 2 or 
** greater and the function pointer is not null) and will fall back
** to xcurrenttime() if xcurrenttimeint64() is unavailable.
**
** ^the xsetsystemcall(), xgetsystemcall(), and xnestsystemcall() interfaces
** are not used by the sqlite core.  these optional interfaces are provided
** by some vfses to facilitate testing of the vfs code. by overriding 
** system calls with functions under its control, a test program can
** simulate faults and error conditions that would otherwise be difficult
** or impossible to induce.  the set of system calls that can be overridden
** varies from one vfs to another, and from one version of the same vfs to the
** next.  applications that use these interfaces must be prepared for any
** or all of these interfaces to be null or for their behavior to change
** from one release to the next.  applications must not attempt to access
** any of these methods if the iversion of the vfs is less than 3.
*/
typedef struct sqlite3_vfs sqlite3_vfs;
typedef void (*sqlite3_syscall_ptr)(void);
struct sqlite3_vfs {
  int iversion;            /* structure version number (currently 3) */
  int szosfile;            /* size of subclassed sqlite3_file */
  int mxpathname;          /* maximum file pathname length */
  sqlite3_vfs *pnext;      /* next registered vfs */
  const char *zname;       /* name of this virtual file system */
  void *pappdata;          /* pointer to application-specific data */
  int (*xopen)(sqlite3_vfs*, const char *zname, sqlite3_file*,
               int flags, int *poutflags);
  int (*xdelete)(sqlite3_vfs*, const char *zname, int syncdir);
  int (*xaccess)(sqlite3_vfs*, const char *zname, int flags, int *presout);
  int (*xfullpathname)(sqlite3_vfs*, const char *zname, int nout, char *zout);
  void *(*xdlopen)(sqlite3_vfs*, const char *zfilename);
  void (*xdlerror)(sqlite3_vfs*, int nbyte, char *zerrmsg);
  void (*(*xdlsym)(sqlite3_vfs*,void*, const char *zsymbol))(void);
  void (*xdlclose)(sqlite3_vfs*, void*);
  int (*xrandomness)(sqlite3_vfs*, int nbyte, char *zout);
  int (*xsleep)(sqlite3_vfs*, int microseconds);
  int (*xcurrenttime)(sqlite3_vfs*, double*);
  int (*xgetlasterror)(sqlite3_vfs*, int, char *);
  /*
  ** the methods above are in version 1 of the sqlite_vfs object
  ** definition.  those that follow are added in version 2 or later
  */
  int (*xcurrenttimeint64)(sqlite3_vfs*, sqlite3_int64*);
  /*
  ** the methods above are in versions 1 and 2 of the sqlite_vfs object.
  ** those below are for version 3 and greater.
  */
  int (*xsetsystemcall)(sqlite3_vfs*, const char *zname, sqlite3_syscall_ptr);
  sqlite3_syscall_ptr (*xgetsystemcall)(sqlite3_vfs*, const char *zname);
  const char *(*xnextsystemcall)(sqlite3_vfs*, const char *zname);
  /*
  ** the methods above are in versions 1 through 3 of the sqlite_vfs object.
  ** new fields may be appended in figure versions.  the iversion
  ** value will increment whenever this happens. 
  */
};

/*
** capi3ref: flags for the xaccess vfs method
**
** these integer constants can be used as the third parameter to
** the xaccess method of an [sqlite3_vfs] object.  they determine
** what kind of permissions the xaccess method is looking for.
** with sqlite_access_exists, the xaccess method
** simply checks whether the file exists.
** with sqlite_access_readwrite, the xaccess method
** checks whether the named directory is both readable and writable
** (in other words, if files can be added, removed, and renamed within
** the directory).
** the sqlite_access_readwrite constant is currently used only by the
** [temp_store_directory pragma], though this could change in a future
** release of sqlite.
** with sqlite_access_read, the xaccess method
** checks whether the file is readable.  the sqlite_access_read constant is
** currently unused, though it might be used in a future release of
** sqlite.
*/
#define sqlite_access_exists    0
#define sqlite_access_readwrite 1   /* used by pragma temp_store_directory */
#define sqlite_access_read      2   /* unused */

/*
** capi3ref: flags for the xshmlock vfs method
**
** these integer constants define the various locking operations
** allowed by the xshmlock method of [sqlite3_io_methods].  the
** following are the only legal combinations of flags to the
** xshmlock method:
**
** <ul>
** <li>  sqlite_shm_lock | sqlite_shm_shared
** <li>  sqlite_shm_lock | sqlite_shm_exclusive
** <li>  sqlite_shm_unlock | sqlite_shm_shared
** <li>  sqlite_shm_unlock | sqlite_shm_exclusive
** </ul>
**
** when unlocking, the same shared or exclusive flag must be supplied as
** was given no the corresponding lock.  
**
** the xshmlock method can transition between unlocked and shared or
** between unlocked and exclusive.  it cannot transition between shared
** and exclusive.
*/
#define sqlite_shm_unlock       1
#define sqlite_shm_lock         2
#define sqlite_shm_shared       4
#define sqlite_shm_exclusive    8

/*
** capi3ref: maximum xshmlock index
**
** the xshmlock method on [sqlite3_io_methods] may use values
** between 0 and this upper bound as its "offset" argument.
** the sqlite core will never attempt to acquire or release a
** lock outside of this range
*/
#define sqlite_shm_nlock        8


/*
** capi3ref: initialize the sqlite library
**
** ^the sqlite3_initialize() routine initializes the
** sqlite library.  ^the sqlite3_shutdown() routine
** deallocates any resources that were allocated by sqlite3_initialize().
** these routines are designed to aid in process initialization and
** shutdown on embedded systems.  workstation applications using
** sqlite normally do not need to invoke either of these routines.
**
** a call to sqlite3_initialize() is an "effective" call if it is
** the first time sqlite3_initialize() is invoked during the lifetime of
** the process, or if it is the first time sqlite3_initialize() is invoked
** following a call to sqlite3_shutdown().  ^(only an effective call
** of sqlite3_initialize() does any initialization.  all other calls
** are harmless no-ops.)^
**
** a call to sqlite3_shutdown() is an "effective" call if it is the first
** call to sqlite3_shutdown() since the last sqlite3_initialize().  ^(only
** an effective call to sqlite3_shutdown() does any deinitialization.
** all other valid calls to sqlite3_shutdown() are harmless no-ops.)^
**
** the sqlite3_initialize() interface is threadsafe, but sqlite3_shutdown()
** is not.  the sqlite3_shutdown() interface must only be called from a
** single thread.  all open [database connections] must be closed and all
** other sqlite resources must be deallocated prior to invoking
** sqlite3_shutdown().
**
** among other things, ^sqlite3_initialize() will invoke
** sqlite3_os_init().  similarly, ^sqlite3_shutdown()
** will invoke sqlite3_os_end().
**
** ^the sqlite3_initialize() routine returns [sqlite_ok] on success.
** ^if for some reason, sqlite3_initialize() is unable to initialize
** the library (perhaps it is unable to allocate a needed resource such
** as a mutex) it returns an [error code] other than [sqlite_ok].
**
** ^the sqlite3_initialize() routine is called internally by many other
** sqlite interfaces so that an application usually does not need to
** invoke sqlite3_initialize() directly.  for example, [sqlite3_open()]
** calls sqlite3_initialize() so the sqlite library will be automatically
** initialized when [sqlite3_open()] is called if it has not be initialized
** already.  ^however, if sqlite is compiled with the [sqlite_omit_autoinit]
** compile-time option, then the automatic calls to sqlite3_initialize()
** are omitted and the application must call sqlite3_initialize() directly
** prior to using any other sqlite interface.  for maximum portability,
** it is recommended that applications always invoke sqlite3_initialize()
** directly prior to using any other sqlite interface.  future releases
** of sqlite may require this.  in other words, the behavior exhibited
** when sqlite is compiled with [sqlite_omit_autoinit] might become the
** default behavior in some future release of sqlite.
**
** the sqlite3_os_init() routine does operating-system specific
** initialization of the sqlite library.  the sqlite3_os_end()
** routine undoes the effect of sqlite3_os_init().  typical tasks
** performed by these routines include allocation or deallocation
** of static resources, initialization of global variables,
** setting up a default [sqlite3_vfs] module, or setting up
** a default configuration using [sqlite3_config()].
**
** the application should never invoke either sqlite3_os_init()
** or sqlite3_os_end() directly.  the application should only invoke
** sqlite3_initialize() and sqlite3_shutdown().  the sqlite3_os_init()
** interface is called automatically by sqlite3_initialize() and
** sqlite3_os_end() is called by sqlite3_shutdown().  appropriate
** implementations for sqlite3_os_init() and sqlite3_os_end()
** are built into sqlite when it is compiled for unix, windows, or os/2.
** when [custom builds | built for other platforms]
** (using the [sqlite_os_other=1] compile-time
** option) the application must supply a suitable implementation for
** sqlite3_os_init() and sqlite3_os_end().  an application-supplied
** implementation of sqlite3_os_init() or sqlite3_os_end()
** must return [sqlite_ok] on success and some other [error code] upon
** failure.
*/
sqlite_api int sqlite3_initialize(void);
sqlite_api int sqlite3_shutdown(void);
sqlite_api int sqlite3_os_init(void);
sqlite_api int sqlite3_os_end(void);

/*
** capi3ref: configuring the sqlite library
**
** the sqlite3_config() interface is used to make global configuration
** changes to sqlite in order to tune sqlite to the specific needs of
** the application.  the default configuration is recommended for most
** applications and so this routine is usually not necessary.  it is
** provided to support rare applications with unusual needs.
**
** the sqlite3_config() interface is not threadsafe.  the application
** must insure that no other sqlite interfaces are invoked by other
** threads while sqlite3_config() is running.  furthermore, sqlite3_config()
** may only be invoked prior to library initialization using
** [sqlite3_initialize()] or after shutdown by [sqlite3_shutdown()].
** ^if sqlite3_config() is called after [sqlite3_initialize()] and before
** [sqlite3_shutdown()] then it will return sqlite_misuse.
** note, however, that ^sqlite3_config() can be called as part of the
** implementation of an application-defined [sqlite3_os_init()].
**
** the first argument to sqlite3_config() is an integer
** [configuration option] that determines
** what property of sqlite is to be configured.  subsequent arguments
** vary depending on the [configuration option]
** in the first argument.
**
** ^when a configuration option is set, sqlite3_config() returns [sqlite_ok].
** ^if the option is unknown or sqlite is unable to set the option
** then this routine returns a non-zero [error code].
*/
sqlite_api int sqlite3_config(int, ...);

/*
** capi3ref: configure database connections
**
** the sqlite3_db_config() interface is used to make configuration
** changes to a [database connection].  the interface is similar to
** [sqlite3_config()] except that the changes apply to a single
** [database connection] (specified in the first argument).
**
** the second argument to sqlite3_db_config(d,v,...)  is the
** [sqlite_dbconfig_lookaside | configuration verb] - an integer code 
** that indicates what aspect of the [database connection] is being configured.
** subsequent arguments vary depending on the configuration verb.
**
** ^calls to sqlite3_db_config() return sqlite_ok if and only if
** the call is considered successful.
*/
sqlite_api int sqlite3_db_config(sqlite3*, int op, ...);

/*
** capi3ref: memory allocation routines
**
** an instance of this object defines the interface between sqlite
** and low-level memory allocation routines.
**
** this object is used in only one place in the sqlite interface.
** a pointer to an instance of this object is the argument to
** [sqlite3_config()] when the configuration option is
** [sqlite_config_malloc] or [sqlite_config_getmalloc].  
** by creating an instance of this object
** and passing it to [sqlite3_config]([sqlite_config_malloc])
** during configuration, an application can specify an alternative
** memory allocation subsystem for sqlite to use for all of its
** dynamic memory needs.
**
** note that sqlite comes with several [built-in memory allocators]
** that are perfectly adequate for the overwhelming majority of applications
** and that this object is only useful to a tiny minority of applications
** with specialized memory allocation requirements.  this object is
** also used during testing of sqlite in order to specify an alternative
** memory allocator that simulates memory out-of-memory conditions in
** order to verify that sqlite recovers gracefully from such
** conditions.
**
** the xmalloc, xrealloc, and xfree methods must work like the
** malloc(), realloc() and free() functions from the standard c library.
** ^sqlite guarantees that the second argument to
** xrealloc is always a value returned by a prior call to xroundup.
**
** xsize should return the allocated size of a memory allocation
** previously obtained from xmalloc or xrealloc.  the allocated size
** is always at least as big as the requested size but may be larger.
**
** the xroundup method returns what would be the allocated size of
** a memory allocation given a particular requested size.  most memory
** allocators round up memory allocations at least to the next multiple
** of 8.  some allocators round up to a larger multiple or to a power of 2.
** every memory allocation request coming in through [sqlite3_malloc()]
** or [sqlite3_realloc()] first calls xroundup.  if xroundup returns 0, 
** that causes the corresponding memory allocation to fail.
**
** the xinit method initializes the memory allocator.  for example,
** it might allocate any require mutexes or initialize internal data
** structures.  the xshutdown method is invoked (indirectly) by
** [sqlite3_shutdown()] and should deallocate any resources acquired
** by xinit.  the pappdata pointer is used as the only parameter to
** xinit and xshutdown.
**
** sqlite holds the [sqlite_mutex_static_master] mutex when it invokes
** the xinit method, so the xinit method need not be threadsafe.  the
** xshutdown method is only called from [sqlite3_shutdown()] so it does
** not need to be threadsafe either.  for all other methods, sqlite
** holds the [sqlite_mutex_static_mem] mutex as long as the
** [sqlite_config_memstatus] configuration option is turned on (which
** it is by default) and so the methods are automatically serialized.
** however, if [sqlite_config_memstatus] is disabled, then the other
** methods must be threadsafe or else make their own arrangements for
** serialization.
**
** sqlite will never invoke xinit() more than once without an intervening
** call to xshutdown().
*/
typedef struct sqlite3_mem_methods sqlite3_mem_methods;
struct sqlite3_mem_methods {
  void *(*xmalloc)(int);         /* memory allocation function */
  void (*xfree)(void*);          /* free a prior allocation */
  void *(*xrealloc)(void*,int);  /* resize an allocation */
  int (*xsize)(void*);           /* return the size of an allocation */
  int (*xroundup)(int);          /* round up request size to allocation size */
  int (*xinit)(void*);           /* initialize the memory allocator */
  void (*xshutdown)(void*);      /* deinitialize the memory allocator */
  void *pappdata;                /* argument to xinit() and xshutdown() */
};

/*
** capi3ref: configuration options
** keywords: {configuration option}
**
** these constants are the available integer configuration options that
** can be passed as the first argument to the [sqlite3_config()] interface.
**
** new configuration options may be added in future releases of sqlite.
** existing configuration options might be discontinued.  applications
** should check the return code from [sqlite3_config()] to make sure that
** the call worked.  the [sqlite3_config()] interface will return a
** non-zero [error code] if a discontinued or unsupported configuration option
** is invoked.
**
** <dl>
** [[sqlite_config_singlethread]] <dt>sqlite_config_singlethread</dt>
** <dd>there are no arguments to this option.  ^this option sets the
** [threading mode] to single-thread.  in other words, it disables
** all mutexing and puts sqlite into a mode where it can only be used
** by a single thread.   ^if sqlite is compiled with
** the [sqlite_threadsafe | sqlite_threadsafe=0] compile-time option then
** it is not possible to change the [threading mode] from its default
** value of single-thread and so [sqlite3_config()] will return 
** [sqlite_error] if called with the sqlite_config_singlethread
** configuration option.</dd>
**
** [[sqlite_config_multithread]] <dt>sqlite_config_multithread</dt>
** <dd>there are no arguments to this option.  ^this option sets the
** [threading mode] to multi-thread.  in other words, it disables
** mutexing on [database connection] and [prepared statement] objects.
** the application is responsible for serializing access to
** [database connections] and [prepared statements].  but other mutexes
** are enabled so that sqlite will be safe to use in a multi-threaded
** environment as long as no two threads attempt to use the same
** [database connection] at the same time.  ^if sqlite is compiled with
** the [sqlite_threadsafe | sqlite_threadsafe=0] compile-time option then
** it is not possible to set the multi-thread [threading mode] and
** [sqlite3_config()] will return [sqlite_error] if called with the
** sqlite_config_multithread configuration option.</dd>
**
** [[sqlite_config_serialized]] <dt>sqlite_config_serialized</dt>
** <dd>there are no arguments to this option.  ^this option sets the
** [threading mode] to serialized. in other words, this option enables
** all mutexes including the recursive
** mutexes on [database connection] and [prepared statement] objects.
** in this mode (which is the default when sqlite is compiled with
** [sqlite_threadsafe=1]) the sqlite library will itself serialize access
** to [database connections] and [prepared statements] so that the
** application is free to use the same [database connection] or the
** same [prepared statement] in different threads at the same time.
** ^if sqlite is compiled with
** the [sqlite_threadsafe | sqlite_threadsafe=0] compile-time option then
** it is not possible to set the serialized [threading mode] and
** [sqlite3_config()] will return [sqlite_error] if called with the
** sqlite_config_serialized configuration option.</dd>
**
** [[sqlite_config_malloc]] <dt>sqlite_config_malloc</dt>
** <dd> ^(this option takes a single argument which is a pointer to an
** instance of the [sqlite3_mem_methods] structure.  the argument specifies
** alternative low-level memory allocation routines to be used in place of
** the memory allocation routines built into sqlite.)^ ^sqlite makes
** its own private copy of the content of the [sqlite3_mem_methods] structure
** before the [sqlite3_config()] call returns.</dd>
**
** [[sqlite_config_getmalloc]] <dt>sqlite_config_getmalloc</dt>
** <dd> ^(this option takes a single argument which is a pointer to an
** instance of the [sqlite3_mem_methods] structure.  the [sqlite3_mem_methods]
** structure is filled with the currently defined memory allocation routines.)^
** this option can be used to overload the default memory allocation
** routines with a wrapper that simulations memory allocation failure or
** tracks memory usage, for example. </dd>
**
** [[sqlite_config_memstatus]] <dt>sqlite_config_memstatus</dt>
** <dd> ^this option takes single argument of type int, interpreted as a 
** boolean, which enables or disables the collection of memory allocation 
** statistics. ^(when memory allocation statistics are disabled, the 
** following sqlite interfaces become non-operational:
**   <ul>
**   <li> [sqlite3_memory_used()]
**   <li> [sqlite3_memory_highwater()]
**   <li> [sqlite3_soft_heap_limit64()]
**   <li> [sqlite3_status()]
**   </ul>)^
** ^memory allocation statistics are enabled by default unless sqlite is
** compiled with [sqlite_default_memstatus]=0 in which case memory
** allocation statistics are disabled by default.
** </dd>
**
** [[sqlite_config_scratch]] <dt>sqlite_config_scratch</dt>
** <dd> ^this option specifies a static memory buffer that sqlite can use for
** scratch memory.  there are three arguments:  a pointer an 8-byte
** aligned memory buffer from which the scratch allocations will be
** drawn, the size of each scratch allocation (sz),
** and the maximum number of scratch allocations (n).  the sz
** argument must be a multiple of 16.
** the first argument must be a pointer to an 8-byte aligned buffer
** of at least sz*n bytes of memory.
** ^sqlite will use no more than two scratch buffers per thread.  so
** n should be set to twice the expected maximum number of threads.
** ^sqlite will never require a scratch buffer that is more than 6
** times the database page size. ^if sqlite needs needs additional
** scratch memory beyond what is provided by this configuration option, then 
** [sqlite3_malloc()] will be used to obtain the memory needed.</dd>
**
** [[sqlite_config_pagecache]] <dt>sqlite_config_pagecache</dt>
** <dd> ^this option specifies a static memory buffer that sqlite can use for
** the database page cache with the default page cache implementation.  
** this configuration should not be used if an application-define page
** cache implementation is loaded using the sqlite_config_pcache2 option.
** there are three arguments to this option: a pointer to 8-byte aligned
** memory, the size of each page buffer (sz), and the number of pages (n).
** the sz argument should be the size of the largest database page
** (a power of two between 512 and 32768) plus a little extra for each
** page header.  ^the page header size is 20 to 40 bytes depending on
** the host architecture.  ^it is harmless, apart from the wasted memory,
** to make sz a little too large.  the first
** argument should point to an allocation of at least sz*n bytes of memory.
** ^sqlite will use the memory provided by the first argument to satisfy its
** memory needs for the first n pages that it adds to cache.  ^if additional
** page cache memory is needed beyond what is provided by this option, then
** sqlite goes to [sqlite3_malloc()] for the additional storage space.
** the pointer in the first argument must
** be aligned to an 8-byte boundary or subsequent behavior of sqlite
** will be undefined.</dd>
**
** [[sqlite_config_heap]] <dt>sqlite_config_heap</dt>
** <dd> ^this option specifies a static memory buffer that sqlite will use
** for all of its dynamic memory allocation needs beyond those provided
** for by [sqlite_config_scratch] and [sqlite_config_pagecache].
** there are three arguments: an 8-byte aligned pointer to the memory,
** the number of bytes in the memory buffer, and the minimum allocation size.
** ^if the first pointer (the memory pointer) is null, then sqlite reverts
** to using its default memory allocator (the system malloc() implementation),
** undoing any prior invocation of [sqlite_config_malloc].  ^if the
** memory pointer is not null and either [sqlite_enable_memsys3] or
** [sqlite_enable_memsys5] are defined, then the alternative memory
** allocator is engaged to handle all of sqlites memory allocation needs.
** the first pointer (the memory pointer) must be aligned to an 8-byte
** boundary or subsequent behavior of sqlite will be undefined.
** the minimum allocation size is capped at 2**12. reasonable values
** for the minimum allocation size are 2**5 through 2**8.</dd>
**
** [[sqlite_config_mutex]] <dt>sqlite_config_mutex</dt>
** <dd> ^(this option takes a single argument which is a pointer to an
** instance of the [sqlite3_mutex_methods] structure.  the argument specifies
** alternative low-level mutex routines to be used in place
** the mutex routines built into sqlite.)^  ^sqlite makes a copy of the
** content of the [sqlite3_mutex_methods] structure before the call to
** [sqlite3_config()] returns. ^if sqlite is compiled with
** the [sqlite_threadsafe | sqlite_threadsafe=0] compile-time option then
** the entire mutexing subsystem is omitted from the build and hence calls to
** [sqlite3_config()] with the sqlite_config_mutex configuration option will
** return [sqlite_error].</dd>
**
** [[sqlite_config_getmutex]] <dt>sqlite_config_getmutex</dt>
** <dd> ^(this option takes a single argument which is a pointer to an
** instance of the [sqlite3_mutex_methods] structure.  the
** [sqlite3_mutex_methods]
** structure is filled with the currently defined mutex routines.)^
** this option can be used to overload the default mutex allocation
** routines with a wrapper used to track mutex usage for performance
** profiling or testing, for example.   ^if sqlite is compiled with
** the [sqlite_threadsafe | sqlite_threadsafe=0] compile-time option then
** the entire mutexing subsystem is omitted from the build and hence calls to
** [sqlite3_config()] with the sqlite_config_getmutex configuration option will
** return [sqlite_error].</dd>
**
** [[sqlite_config_lookaside]] <dt>sqlite_config_lookaside</dt>
** <dd> ^(this option takes two arguments that determine the default
** memory allocation for the lookaside memory allocator on each
** [database connection].  the first argument is the
** size of each lookaside buffer slot and the second is the number of
** slots allocated to each database connection.)^  ^(this option sets the
** <i>default</i> lookaside size. the [sqlite_dbconfig_lookaside]
** verb to [sqlite3_db_config()] can be used to change the lookaside
** configuration on individual connections.)^ </dd>
**
** [[sqlite_config_pcache2]] <dt>sqlite_config_pcache2</dt>
** <dd> ^(this option takes a single argument which is a pointer to
** an [sqlite3_pcache_methods2] object.  this object specifies the interface
** to a custom page cache implementation.)^  ^sqlite makes a copy of the
** object and uses it for page cache memory allocations.</dd>
**
** [[sqlite_config_getpcache2]] <dt>sqlite_config_getpcache2</dt>
** <dd> ^(this option takes a single argument which is a pointer to an
** [sqlite3_pcache_methods2] object.  sqlite copies of the current
** page cache implementation into that object.)^ </dd>
**
** [[sqlite_config_log]] <dt>sqlite_config_log</dt>
** <dd> the sqlite_config_log option is used to configure the sqlite
** global [error log].
** (^the sqlite_config_log option takes two arguments: a pointer to a
** function with a call signature of void(*)(void*,int,const char*), 
** and a pointer to void. ^if the function pointer is not null, it is
** invoked by [sqlite3_log()] to process each logging event.  ^if the
** function pointer is null, the [sqlite3_log()] interface becomes a no-op.
** ^the void pointer that is the second argument to sqlite_config_log is
** passed through as the first parameter to the application-defined logger
** function whenever that function is invoked.  ^the second parameter to
** the logger function is a copy of the first parameter to the corresponding
** [sqlite3_log()] call and is intended to be a [result code] or an
** [extended result code].  ^the third parameter passed to the logger is
** log message after formatting via [sqlite3_snprintf()].
** the sqlite logging interface is not reentrant; the logger function
** supplied by the application must not invoke any sqlite interface.
** in a multi-threaded application, the application-defined logger
** function must be threadsafe. </dd>
**
** [[sqlite_config_uri]] <dt>sqlite_config_uri
** <dd>^(this option takes a single argument of type int. if non-zero, then
** uri handling is globally enabled. if the parameter is zero, then uri handling
** is globally disabled.)^ ^if uri handling is globally enabled, all filenames
** passed to [sqlite3_open()], [sqlite3_open_v2()], [sqlite3_open16()] or
** specified as part of [attach] commands are interpreted as uris, regardless
** of whether or not the [sqlite_open_uri] flag is set when the database
** connection is opened. ^if it is globally disabled, filenames are
** only interpreted as uris if the sqlite_open_uri flag is set when the
** database connection is opened. ^(by default, uri handling is globally
** disabled. the default value may be changed by compiling with the
** [sqlite_use_uri] symbol defined.)^
**
** [[sqlite_config_covering_index_scan]] <dt>sqlite_config_covering_index_scan
** <dd>^this option takes a single integer argument which is interpreted as
** a boolean in order to enable or disable the use of covering indices for
** full table scans in the query optimizer.  ^the default setting is determined
** by the [sqlite_allow_covering_index_scan] compile-time option, or is "on"
** if that compile-time option is omitted.
** the ability to disable the use of covering indices for full table scans
** is because some incorrectly coded legacy applications might malfunction
** when the optimization is enabled.  providing the ability to
** disable the optimization allows the older, buggy application code to work
** without change even with newer versions of sqlite.
**
** [[sqlite_config_pcache]] [[sqlite_config_getpcache]]
** <dt>sqlite_config_pcache and sqlite_config_getpcache
** <dd> these options are obsolete and should not be used by new code.
** they are retained for backwards compatibility but are now no-ops.
** </dd>
**
** [[sqlite_config_sqllog]]
** <dt>sqlite_config_sqllog
** <dd>this option is only available if sqlite is compiled with the
** [sqlite_enable_sqllog] pre-processor macro defined. the first argument should
** be a pointer to a function of type void(*)(void*,sqlite3*,const char*, int).
** the second should be of type (void*). the callback is invoked by the library
** in three separate circumstances, identified by the value passed as the
** fourth parameter. if the fourth parameter is 0, then the database connection
** passed as the second argument has just been opened. the third argument
** points to a buffer containing the name of the main database file. if the
** fourth parameter is 1, then the sql statement that the third parameter
** points to has just been executed. or, if the fourth parameter is 2, then
** the connection being passed as the second parameter is being closed. the
** third parameter is passed null in this case.  an example of using this
** configuration option can be seen in the "test_sqllog.c" source file in
** the canonical sqlite source tree.</dd>
**
** [[sqlite_config_mmap_size]]
** <dt>sqlite_config_mmap_size
** <dd>^sqlite_config_mmap_size takes two 64-bit integer (sqlite3_int64) values
** that are the default mmap size limit (the default setting for
** [pragma mmap_size]) and the maximum allowed mmap size limit.
** ^the default setting can be overridden by each database connection using
** either the [pragma mmap_size] command, or by using the
** [sqlite_fcntl_mmap_size] file control.  ^(the maximum allowed mmap size
** cannot be changed at run-time.  nor may the maximum allowed mmap size
** exceed the compile-time maximum mmap size set by the
** [sqlite_max_mmap_size] compile-time option.)^
** ^if either argument to this option is negative, then that argument is
** changed to its compile-time default.
**
** [[sqlite_config_win32_heapsize]]
** <dt>sqlite_config_win32_heapsize
** <dd>^this option is only available if sqlite is compiled for windows
** with the [sqlite_win32_malloc] pre-processor macro defined.
** sqlite_config_win32_heapsize takes a 32-bit unsigned integer value
** that specifies the maximum size of the created heap.
** </dl>
*/
#define sqlite_config_singlethread  1  /* nil */
#define sqlite_config_multithread   2  /* nil */
#define sqlite_config_serialized    3  /* nil */
#define sqlite_config_malloc        4  /* sqlite3_mem_methods* */
#define sqlite_config_getmalloc     5  /* sqlite3_mem_methods* */
#define sqlite_config_scratch       6  /* void*, int sz, int n */
#define sqlite_config_pagecache     7  /* void*, int sz, int n */
#define sqlite_config_heap          8  /* void*, int nbyte, int min */
#define sqlite_config_memstatus     9  /* boolean */
#define sqlite_config_mutex        10  /* sqlite3_mutex_methods* */
#define sqlite_config_getmutex     11  /* sqlite3_mutex_methods* */
/* previously sqlite_config_chunkalloc 12 which is now unused. */ 
#define sqlite_config_lookaside    13  /* int int */
#define sqlite_config_pcache       14  /* no-op */
#define sqlite_config_getpcache    15  /* no-op */
#define sqlite_config_log          16  /* xfunc, void* */
#define sqlite_config_uri          17  /* int */
#define sqlite_config_pcache2      18  /* sqlite3_pcache_methods2* */
#define sqlite_config_getpcache2   19  /* sqlite3_pcache_methods2* */
#define sqlite_config_covering_index_scan 20  /* int */
#define sqlite_config_sqllog       21  /* xsqllog, void* */
#define sqlite_config_mmap_size    22  /* sqlite3_int64, sqlite3_int64 */
#define sqlite_config_win32_heapsize      23  /* int nbyte */

/*
** capi3ref: database connection configuration options
**
** these constants are the available integer configuration options that
** can be passed as the second argument to the [sqlite3_db_config()] interface.
**
** new configuration options may be added in future releases of sqlite.
** existing configuration options might be discontinued.  applications
** should check the return code from [sqlite3_db_config()] to make sure that
** the call worked.  ^the [sqlite3_db_config()] interface will return a
** non-zero [error code] if a discontinued or unsupported configuration option
** is invoked.
**
** <dl>
** <dt>sqlite_dbconfig_lookaside</dt>
** <dd> ^this option takes three additional arguments that determine the 
** [lookaside memory allocator] configuration for the [database connection].
** ^the first argument (the third parameter to [sqlite3_db_config()] is a
** pointer to a memory buffer to use for lookaside memory.
** ^the first argument after the sqlite_dbconfig_lookaside verb
** may be null in which case sqlite will allocate the
** lookaside buffer itself using [sqlite3_malloc()]. ^the second argument is the
** size of each lookaside buffer slot.  ^the third argument is the number of
** slots.  the size of the buffer in the first argument must be greater than
** or equal to the product of the second and third arguments.  the buffer
** must be aligned to an 8-byte boundary.  ^if the second argument to
** sqlite_dbconfig_lookaside is not a multiple of 8, it is internally
** rounded down to the next smaller multiple of 8.  ^(the lookaside memory
** configuration for a database connection can only be changed when that
** connection is not currently using lookaside memory, or in other words
** when the "current value" returned by
** [sqlite3_db_status](d,[sqlite_config_lookaside],...) is zero.
** any attempt to change the lookaside memory configuration when lookaside
** memory is in use leaves the configuration unchanged and returns 
** [sqlite_busy].)^</dd>
**
** <dt>sqlite_dbconfig_enable_fkey</dt>
** <dd> ^this option is used to enable or disable the enforcement of
** [foreign key constraints].  there should be two additional arguments.
** the first argument is an integer which is 0 to disable fk enforcement,
** positive to enable fk enforcement or negative to leave fk enforcement
** unchanged.  the second parameter is a pointer to an integer into which
** is written 0 or 1 to indicate whether fk enforcement is off or on
** following this call.  the second parameter may be a null pointer, in
** which case the fk enforcement setting is not reported back. </dd>
**
** <dt>sqlite_dbconfig_enable_trigger</dt>
** <dd> ^this option is used to enable or disable [create trigger | triggers].
** there should be two additional arguments.
** the first argument is an integer which is 0 to disable triggers,
** positive to enable triggers or negative to leave the setting unchanged.
** the second parameter is a pointer to an integer into which
** is written 0 or 1 to indicate whether triggers are disabled or enabled
** following this call.  the second parameter may be a null pointer, in
** which case the trigger setting is not reported back. </dd>
**
** </dl>
*/
#define sqlite_dbconfig_lookaside       1001  /* void* int int */
#define sqlite_dbconfig_enable_fkey     1002  /* int int* */
#define sqlite_dbconfig_enable_trigger  1003  /* int int* */


/*
** capi3ref: enable or disable extended result codes
**
** ^the sqlite3_extended_result_codes() routine enables or disables the
** [extended result codes] feature of sqlite. ^the extended result
** codes are disabled by default for historical compatibility.
*/
sqlite_api int sqlite3_extended_result_codes(sqlite3*, int onoff);

/*
** capi3ref: last insert rowid
**
** ^each entry in most sqlite tables (except for [without rowid] tables)
** has a unique 64-bit signed
** integer key called the [rowid | "rowid"]. ^the rowid is always available
** as an undeclared column named rowid, oid, or _rowid_ as long as those
** names are not also used by explicitly declared columns. ^if
** the table has a column of type [integer primary key] then that column
** is another alias for the rowid.
**
** ^the sqlite3_last_insert_rowid(d) interface returns the [rowid] of the 
** most recent successful [insert] into a rowid table or [virtual table]
** on database connection d.
** ^inserts into [without rowid] tables are not recorded.
** ^if no successful [insert]s into rowid tables
** have ever occurred on the database connection d, 
** then sqlite3_last_insert_rowid(d) returns zero.
**
** ^(if an [insert] occurs within a trigger or within a [virtual table]
** method, then this routine will return the [rowid] of the inserted
** row as long as the trigger or virtual table method is running.
** but once the trigger or virtual table method ends, the value returned 
** by this routine reverts to what it was before the trigger or virtual
** table method began.)^
**
** ^an [insert] that fails due to a constraint violation is not a
** successful [insert] and does not change the value returned by this
** routine.  ^thus insert or fail, insert or ignore, insert or rollback,
** and insert or abort make no changes to the return value of this
** routine when their insertion fails.  ^(when insert or replace
** encounters a constraint violation, it does not fail.  the
** insert continues to completion after deleting rows that caused
** the constraint problem so insert or replace will always change
** the return value of this interface.)^
**
** ^for the purposes of this routine, an [insert] is considered to
** be successful even if it is subsequently rolled back.
**
** this function is accessible to sql statements via the
** [last_insert_rowid() sql function].
**
** if a separate thread performs a new [insert] on the same
** database connection while the [sqlite3_last_insert_rowid()]
** function is running and thus changes the last insert [rowid],
** then the value returned by [sqlite3_last_insert_rowid()] is
** unpredictable and might not equal either the old or the new
** last insert [rowid].
*/
sqlite_api sqlite3_int64 sqlite3_last_insert_rowid(sqlite3*);

/*
** capi3ref: count the number of rows modified
**
** ^this function returns the number of database rows that were changed
** or inserted or deleted by the most recently completed sql statement
** on the [database connection] specified by the first parameter.
** ^(only changes that are directly specified by the [insert], [update],
** or [delete] statement are counted.  auxiliary changes caused by
** triggers or [foreign key actions] are not counted.)^ use the
** [sqlite3_total_changes()] function to find the total number of changes
** including changes caused by triggers and foreign key actions.
**
** ^changes to a view that are simulated by an [instead of trigger]
** are not counted.  only real table changes are counted.
**
** ^(a "row change" is a change to a single row of a single table
** caused by an insert, delete, or update statement.  rows that
** are changed as side effects of [replace] constraint resolution,
** rollback, abort processing, [drop table], or by any other
** mechanisms do not count as direct row changes.)^
**
** a "trigger context" is a scope of execution that begins and
** ends with the script of a [create trigger | trigger]. 
** most sql statements are
** evaluated outside of any trigger.  this is the "top level"
** trigger context.  if a trigger fires from the top level, a
** new trigger context is entered for the duration of that one
** trigger.  subtriggers create subcontexts for their duration.
**
** ^calling [sqlite3_exec()] or [sqlite3_step()] recursively does
** not create a new trigger context.
**
** ^this function returns the number of direct row changes in the
** most recent insert, update, or delete statement within the same
** trigger context.
**
** ^thus, when called from the top level, this function returns the
** number of changes in the most recent insert, update, or delete
** that also occurred at the top level.  ^(within the body of a trigger,
** the sqlite3_changes() interface can be called to find the number of
** changes in the most recently completed insert, update, or delete
** statement within the body of the same trigger.
** however, the number returned does not include changes
** caused by subtriggers since those have their own context.)^
**
** see also the [sqlite3_total_changes()] interface, the
** [count_changes pragma], and the [changes() sql function].
**
** if a separate thread makes changes on the same database connection
** while [sqlite3_changes()] is running then the value returned
** is unpredictable and not meaningful.
*/
sqlite_api int sqlite3_changes(sqlite3*);

/*
** capi3ref: total number of rows modified
**
** ^this function returns the number of row changes caused by [insert],
** [update] or [delete] statements since the [database connection] was opened.
** ^(the count returned by sqlite3_total_changes() includes all changes
** from all [create trigger | trigger] contexts and changes made by
** [foreign key actions]. however,
** the count does not include changes used to implement [replace] constraints,
** do rollbacks or abort processing, or [drop table] processing.  the
** count does not include rows of views that fire an [instead of trigger],
** though if the instead of trigger makes changes of its own, those changes 
** are counted.)^
** ^the sqlite3_total_changes() function counts the changes as soon as
** the statement that makes them is completed (when the statement handle
** is passed to [sqlite3_reset()] or [sqlite3_finalize()]).
**
** see also the [sqlite3_changes()] interface, the
** [count_changes pragma], and the [total_changes() sql function].
**
** if a separate thread makes changes on the same database connection
** while [sqlite3_total_changes()] is running then the value
** returned is unpredictable and not meaningful.
*/
sqlite_api int sqlite3_total_changes(sqlite3*);

/*
** capi3ref: interrupt a long-running query
**
** ^this function causes any pending database operation to abort and
** return at its earliest opportunity. this routine is typically
** called in response to a user action such as pressing "cancel"
** or ctrl-c where the user wants a long query operation to halt
** immediately.
**
** ^it is safe to call this routine from a thread different from the
** thread that is currently running the database operation.  but it
** is not safe to call this routine with a [database connection] that
** is closed or might close before sqlite3_interrupt() returns.
**
** ^if an sql operation is very nearly finished at the time when
** sqlite3_interrupt() is called, then it might not have an opportunity
** to be interrupted and might continue to completion.
**
** ^an sql operation that is interrupted will return [sqlite_interrupt].
** ^if the interrupted sql operation is an insert, update, or delete
** that is inside an explicit transaction, then the entire transaction
** will be rolled back automatically.
**
** ^the sqlite3_interrupt(d) call is in effect until all currently running
** sql statements on [database connection] d complete.  ^any new sql statements
** that are started after the sqlite3_interrupt() call and before the 
** running statements reaches zero are interrupted as if they had been
** running prior to the sqlite3_interrupt() call.  ^new sql statements
** that are started after the running statement count reaches zero are
** not effected by the sqlite3_interrupt().
** ^a call to sqlite3_interrupt(d) that occurs when there are no running
** sql statements is a no-op and has no effect on sql statements
** that are started after the sqlite3_interrupt() call returns.
**
** if the database connection closes while [sqlite3_interrupt()]
** is running then bad things will likely happen.
*/
sqlite_api void sqlite3_interrupt(sqlite3*);

/*
** capi3ref: determine if an sql statement is complete
**
** these routines are useful during command-line input to determine if the
** currently entered text seems to form a complete sql statement or
** if additional input is needed before sending the text into
** sqlite for parsing.  ^these routines return 1 if the input string
** appears to be a complete sql statement.  ^a statement is judged to be
** complete if it ends with a semicolon token and is not a prefix of a
** well-formed create trigger statement.  ^semicolons that are embedded within
** string literals or quoted identifier names or comments are not
** independent tokens (they are part of the token in which they are
** embedded) and thus do not count as a statement terminator.  ^whitespace
** and comments that follow the final semicolon are ignored.
**
** ^these routines return 0 if the statement is incomplete.  ^if a
** memory allocation fails, then sqlite_nomem is returned.
**
** ^these routines do not parse the sql statements thus
** will not detect syntactically incorrect sql.
**
** ^(if sqlite has not been initialized using [sqlite3_initialize()] prior 
** to invoking sqlite3_complete16() then sqlite3_initialize() is invoked
** automatically by sqlite3_complete16().  if that initialization fails,
** then the return value from sqlite3_complete16() will be non-zero
** regardless of whether or not the input sql is complete.)^
**
** the input to [sqlite3_complete()] must be a zero-terminated
** utf-8 string.
**
** the input to [sqlite3_complete16()] must be a zero-terminated
** utf-16 string in native byte order.
*/
sqlite_api int sqlite3_complete(const char *sql);
sqlite_api int sqlite3_complete16(const void *sql);

/*
** capi3ref: register a callback to handle sqlite_busy errors
**
** ^the sqlite3_busy_handler(d,x,p) routine sets a callback function x
** that might be invoked with argument p whenever
** an attempt is made to access a database table associated with
** [database connection] d when another thread
** or process has the table locked.
** the sqlite3_busy_handler() interface is used to implement
** [sqlite3_busy_timeout()] and [pragma busy_timeout].
**
** ^if the busy callback is null, then [sqlite_busy]
** is returned immediately upon encountering the lock.  ^if the busy callback
** is not null, then the callback might be invoked with two arguments.
**
** ^the first argument to the busy handler is a copy of the void* pointer which
** is the third argument to sqlite3_busy_handler().  ^the second argument to
** the busy handler callback is the number of times that the busy handler has
** been invoked for the same locking event.  ^if the
** busy callback returns 0, then no additional attempts are made to
** access the database and [sqlite_busy] is returned
** to the application.
** ^if the callback returns non-zero, then another attempt
** is made to access the database and the cycle repeats.
**
** the presence of a busy handler does not guarantee that it will be invoked
** when there is lock contention. ^if sqlite determines that invoking the busy
** handler could result in a deadlock, it will go ahead and return [sqlite_busy]
** to the application instead of invoking the 
** busy handler.
** consider a scenario where one process is holding a read lock that
** it is trying to promote to a reserved lock and
** a second process is holding a reserved lock that it is trying
** to promote to an exclusive lock.  the first process cannot proceed
** because it is blocked by the second and the second process cannot
** proceed because it is blocked by the first.  if both processes
** invoke the busy handlers, neither will make any progress.  therefore,
** sqlite returns [sqlite_busy] for the first process, hoping that this
** will induce the first process to release its read lock and allow
** the second process to proceed.
**
** ^the default busy callback is null.
**
** ^(there can only be a single busy handler defined for each
** [database connection].  setting a new busy handler clears any
** previously set handler.)^  ^note that calling [sqlite3_busy_timeout()]
** or evaluating [pragma busy_timeout=n] will change the
** busy handler and thus clear any previously set busy handler.
**
** the busy callback should not take any actions which modify the
** database connection that invoked the busy handler.  in other words,
** the busy handler is not reentrant.  any such actions
** result in undefined behavior.
** 
** a busy handler must not close the database connection
** or [prepared statement] that invoked the busy handler.
*/
sqlite_api int sqlite3_busy_handler(sqlite3*, int(*)(void*,int), void*);

/*
** capi3ref: set a busy timeout
**
** ^this routine sets a [sqlite3_busy_handler | busy handler] that sleeps
** for a specified amount of time when a table is locked.  ^the handler
** will sleep multiple times until at least "ms" milliseconds of sleeping
** have accumulated.  ^after at least "ms" milliseconds of sleeping,
** the handler returns 0 which causes [sqlite3_step()] to return
** [sqlite_busy].
**
** ^calling this routine with an argument less than or equal to zero
** turns off all busy handlers.
**
** ^(there can only be a single busy handler for a particular
** [database connection] at any given moment.  if another busy handler
** was defined  (using [sqlite3_busy_handler()]) prior to calling
** this routine, that other busy handler is cleared.)^
**
** see also:  [pragma busy_timeout]
*/
sqlite_api int sqlite3_busy_timeout(sqlite3*, int ms);

/*
** capi3ref: convenience routines for running queries
**
** this is a legacy interface that is preserved for backwards compatibility.
** use of this interface is not recommended.
**
** definition: a <b>result table</b> is memory data structure created by the
** [sqlite3_get_table()] interface.  a result table records the
** complete query results from one or more queries.
**
** the table conceptually has a number of rows and columns.  but
** these numbers are not part of the result table itself.  these
** numbers are obtained separately.  let n be the number of rows
** and m be the number of columns.
**
** a result table is an array of pointers to zero-terminated utf-8 strings.
** there are (n+1)*m elements in the array.  the first m pointers point
** to zero-terminated strings that  contain the names of the columns.
** the remaining entries all point to query results.  null values result
** in null pointers.  all other values are in their utf-8 zero-terminated
** string representation as returned by [sqlite3_column_text()].
**
** a result table might consist of one or more memory allocations.
** it is not safe to pass a result table directly to [sqlite3_free()].
** a result table should be deallocated using [sqlite3_free_table()].
**
** ^(as an example of the result table format, suppose a query result
** is as follows:
**
** <blockquote><pre>
**        name        | age
**        -----------------------
**        alice       | 43
**        bob         | 28
**        cindy       | 21
** </pre></blockquote>
**
** there are two column (m==2) and three rows (n==3).  thus the
** result table has 8 entries.  suppose the result table is stored
** in an array names azresult.  then azresult holds this content:
**
** <blockquote><pre>
**        azresult&#91;0] = "name";
**        azresult&#91;1] = "age";
**        azresult&#91;2] = "alice";
**        azresult&#91;3] = "43";
**        azresult&#91;4] = "bob";
**        azresult&#91;5] = "28";
**        azresult&#91;6] = "cindy";
**        azresult&#91;7] = "21";
** </pre></blockquote>)^
**
** ^the sqlite3_get_table() function evaluates one or more
** semicolon-separated sql statements in the zero-terminated utf-8
** string of its 2nd parameter and returns a result table to the
** pointer given in its 3rd parameter.
**
** after the application has finished with the result from sqlite3_get_table(),
** it must pass the result table pointer to sqlite3_free_table() in order to
** release the memory that was malloced.  because of the way the
** [sqlite3_malloc()] happens within sqlite3_get_table(), the calling
** function must not try to call [sqlite3_free()] directly.  only
** [sqlite3_free_table()] is able to release the memory properly and safely.
**
** the sqlite3_get_table() interface is implemented as a wrapper around
** [sqlite3_exec()].  the sqlite3_get_table() routine does not have access
** to any internal data structures of sqlite.  it uses only the public
** interface defined here.  as a consequence, errors that occur in the
** wrapper layer outside of the internal [sqlite3_exec()] call are not
** reflected in subsequent calls to [sqlite3_errcode()] or
** [sqlite3_errmsg()].
*/
sqlite_api int sqlite3_get_table(
  sqlite3 *db,          /* an open database */
  const char *zsql,     /* sql to be evaluated */
  char ***pazresult,    /* results of the query */
  int *pnrow,           /* number of result rows written here */
  int *pncolumn,        /* number of result columns written here */
  char **pzerrmsg       /* error msg written here */
);
sqlite_api void sqlite3_free_table(char **result);

/*
** capi3ref: formatted string printing functions
**
** these routines are work-alikes of the "printf()" family of functions
** from the standard c library.
**
** ^the sqlite3_mprintf() and sqlite3_vmprintf() routines write their
** results into memory obtained from [sqlite3_malloc()].
** the strings returned by these two routines should be
** released by [sqlite3_free()].  ^both routines return a
** null pointer if [sqlite3_malloc()] is unable to allocate enough
** memory to hold the resulting string.
**
** ^(the sqlite3_snprintf() routine is similar to "snprintf()" from
** the standard c library.  the result is written into the
** buffer supplied as the second parameter whose size is given by
** the first parameter. note that the order of the
** first two parameters is reversed from snprintf().)^  this is an
** historical accident that cannot be fixed without breaking
** backwards compatibility.  ^(note also that sqlite3_snprintf()
** returns a pointer to its buffer instead of the number of
** characters actually written into the buffer.)^  we admit that
** the number of characters written would be a more useful return
** value but we cannot change the implementation of sqlite3_snprintf()
** now without breaking compatibility.
**
** ^as long as the buffer size is greater than zero, sqlite3_snprintf()
** guarantees that the buffer is always zero-terminated.  ^the first
** parameter "n" is the total size of the buffer, including space for
** the zero terminator.  so the longest string that can be completely
** written will be n-1 characters.
**
** ^the sqlite3_vsnprintf() routine is a varargs version of sqlite3_snprintf().
**
** these routines all implement some additional formatting
** options that are useful for constructing sql statements.
** all of the usual printf() formatting options apply.  in addition, there
** is are "%q", "%q", and "%z" options.
**
** ^(the %q option works like %s in that it substitutes a nul-terminated
** string from the argument list.  but %q also doubles every '\'' character.
** %q is designed for use inside a string literal.)^  by doubling each '\''
** character it escapes that character and allows it to be inserted into
** the string.
**
** for example, assume the string variable ztext contains text as follows:
**
** <blockquote><pre>
**  char *ztext = "it's a happy day!";
** </pre></blockquote>
**
** one can use this text in an sql statement as follows:
**
** <blockquote><pre>
**  char *zsql = sqlite3_mprintf("insert into table values('%q')", ztext);
**  sqlite3_exec(db, zsql, 0, 0, 0);
**  sqlite3_free(zsql);
** </pre></blockquote>
**
** because the %q format string is used, the '\'' character in ztext
** is escaped and the sql generated is as follows:
**
** <blockquote><pre>
**  insert into table1 values('it''s a happy day!')
** </pre></blockquote>
**
** this is correct.  had we used %s instead of %q, the generated sql
** would have looked like this:
**
** <blockquote><pre>
**  insert into table1 values('it's a happy day!');
** </pre></blockquote>
**
** this second example is an sql syntax error.  as a general rule you should
** always use %q instead of %s when inserting text into a string literal.
**
** ^(the %q option works like %q except it also adds single quotes around
** the outside of the total string.  additionally, if the parameter in the
** argument list is a null pointer, %q substitutes the text "null" (without
** single quotes).)^  so, for example, one could say:
**
** <blockquote><pre>
**  char *zsql = sqlite3_mprintf("insert into table values(%q)", ztext);
**  sqlite3_exec(db, zsql, 0, 0, 0);
**  sqlite3_free(zsql);
** </pre></blockquote>
**
** the code above will render a correct sql statement in the zsql
** variable even if the ztext variable is a null pointer.
**
** ^(the "%z" formatting option works like "%s" but with the
** addition that after the string has been read and copied into
** the result, [sqlite3_free()] is called on the input string.)^
*/
sqlite_api char *sqlite3_mprintf(const char*,...);
sqlite_api char *sqlite3_vmprintf(const char*, va_list);
sqlite_api char *sqlite3_snprintf(int,char*,const char*, ...);
sqlite_api char *sqlite3_vsnprintf(int,char*,const char*, va_list);

/*
** capi3ref: memory allocation subsystem
**
** the sqlite core uses these three routines for all of its own
** internal memory allocation needs. "core" in the previous sentence
** does not include operating-system specific vfs implementation.  the
** windows vfs uses native malloc() and free() for some operations.
**
** ^the sqlite3_malloc() routine returns a pointer to a block
** of memory at least n bytes in length, where n is the parameter.
** ^if sqlite3_malloc() is unable to obtain sufficient free
** memory, it returns a null pointer.  ^if the parameter n to
** sqlite3_malloc() is zero or negative then sqlite3_malloc() returns
** a null pointer.
**
** ^the sqlite3_malloc64(n) routine works just like
** sqlite3_malloc(n) except that n is an unsigned 64-bit integer instead
** of a signed 32-bit integer.
**
** ^calling sqlite3_free() with a pointer previously returned
** by sqlite3_malloc() or sqlite3_realloc() releases that memory so
** that it might be reused.  ^the sqlite3_free() routine is
** a no-op if is called with a null pointer.  passing a null pointer
** to sqlite3_free() is harmless.  after being freed, memory
** should neither be read nor written.  even reading previously freed
** memory might result in a segmentation fault or other severe error.
** memory corruption, a segmentation fault, or other severe error
** might result if sqlite3_free() is called with a non-null pointer that
** was not obtained from sqlite3_malloc() or sqlite3_realloc().
**
** ^the sqlite3_realloc(x,n) interface attempts to resize a
** prior memory allocation x to be at least n bytes.
** ^if the x parameter to sqlite3_realloc(x,n)
** is a null pointer then its behavior is identical to calling
** sqlite3_malloc(n).
** ^if the n parameter to sqlite3_realloc(x,n) is zero or
** negative then the behavior is exactly the same as calling
** sqlite3_free(x).
** ^sqlite3_realloc(x,n) returns a pointer to a memory allocation
** of at least n bytes in size or null if insufficient memory is available.
** ^if m is the size of the prior allocation, then min(n,m) bytes
** of the prior allocation are copied into the beginning of buffer returned
** by sqlite3_realloc(x,n) and the prior allocation is freed.
** ^if sqlite3_realloc(x,n) returns null and n is positive, then the
** prior allocation is not freed.
**
** ^the sqlite3_realloc64(x,n) interfaces works the same as
** sqlite3_realloc(x,n) except that n is a 64-bit unsigned integer instead
** of a 32-bit signed integer.
**
** ^if x is a memory allocation previously obtained from sqlite3_malloc(),
** sqlite3_malloc64(), sqlite3_realloc(), or sqlite3_realloc64(), then
** sqlite3_msize(x) returns the size of that memory allocation in bytes.
** ^the value returned by sqlite3_msize(x) might be larger than the number
** of bytes requested when x was allocated.  ^if x is a null pointer then
** sqlite3_msize(x) returns zero.  if x points to something that is not
** the beginning of memory allocation, or if it points to a formerly
** valid memory allocation that has now been freed, then the behavior
** of sqlite3_msize(x) is undefined and possibly harmful.
**
** ^the memory returned by sqlite3_malloc(), sqlite3_realloc(),
** sqlite3_malloc64(), and sqlite3_realloc64()
** is always aligned to at least an 8 byte boundary, or to a
** 4 byte boundary if the [sqlite_4_byte_aligned_malloc] compile-time
** option is used.
**
** in sqlite version 3.5.0 and 3.5.1, it was possible to define
** the sqlite_omit_memory_allocation which would cause the built-in
** implementation of these routines to be omitted.  that capability
** is no longer provided.  only built-in memory allocators can be used.
**
** prior to sqlite version 3.7.10, the windows os interface layer called
** the system malloc() and free() directly when converting
** filenames between the utf-8 encoding used by sqlite
** and whatever filename encoding is used by the particular windows
** installation.  memory allocation errors were detected, but
** they were reported back as [sqlite_cantopen] or
** [sqlite_ioerr] rather than [sqlite_nomem].
**
** the pointer arguments to [sqlite3_free()] and [sqlite3_realloc()]
** must be either null or else pointers obtained from a prior
** invocation of [sqlite3_malloc()] or [sqlite3_realloc()] that have
** not yet been released.
**
** the application must not read or write any part of
** a block of memory after it has been released using
** [sqlite3_free()] or [sqlite3_realloc()].
*/
sqlite_api void *sqlite3_malloc(int);
sqlite_api void *sqlite3_malloc64(sqlite3_uint64);
sqlite_api void *sqlite3_realloc(void*, int);
sqlite_api void *sqlite3_realloc64(void*, sqlite3_uint64);
sqlite_api void sqlite3_free(void*);
sqlite_api sqlite3_uint64 sqlite3_msize(void*);

/*
** capi3ref: memory allocator statistics
**
** sqlite provides these two interfaces for reporting on the status
** of the [sqlite3_malloc()], [sqlite3_free()], and [sqlite3_realloc()]
** routines, which form the built-in memory allocation subsystem.
**
** ^the [sqlite3_memory_used()] routine returns the number of bytes
** of memory currently outstanding (malloced but not freed).
** ^the [sqlite3_memory_highwater()] routine returns the maximum
** value of [sqlite3_memory_used()] since the high-water mark
** was last reset.  ^the values returned by [sqlite3_memory_used()] and
** [sqlite3_memory_highwater()] include any overhead
** added by sqlite in its implementation of [sqlite3_malloc()],
** but not overhead added by the any underlying system library
** routines that [sqlite3_malloc()] may call.
**
** ^the memory high-water mark is reset to the current value of
** [sqlite3_memory_used()] if and only if the parameter to
** [sqlite3_memory_highwater()] is true.  ^the value returned
** by [sqlite3_memory_highwater(1)] is the high-water mark
** prior to the reset.
*/
sqlite_api sqlite3_int64 sqlite3_memory_used(void);
sqlite_api sqlite3_int64 sqlite3_memory_highwater(int resetflag);

/*
** capi3ref: pseudo-random number generator
**
** sqlite contains a high-quality pseudo-random number generator (prng) used to
** select random [rowid | rowids] when inserting new records into a table that
** already uses the largest possible [rowid].  the prng is also used for
** the build-in random() and randomblob() sql functions.  this interface allows
** applications to access the same prng for other purposes.
**
** ^a call to this routine stores n bytes of randomness into buffer p.
** ^if n is less than one, then p can be a null pointer.
**
** ^if this routine has not been previously called or if the previous
** call had n less than one, then the prng is seeded using randomness
** obtained from the xrandomness method of the default [sqlite3_vfs] object.
** ^if the previous call to this routine had an n of 1 or more then
** the pseudo-randomness is generated
** internally and without recourse to the [sqlite3_vfs] xrandomness
** method.
*/
sqlite_api void sqlite3_randomness(int n, void *p);

/*
** capi3ref: compile-time authorization callbacks
**
** ^this routine registers an authorizer callback with a particular
** [database connection], supplied in the first argument.
** ^the authorizer callback is invoked as sql statements are being compiled
** by [sqlite3_prepare()] or its variants [sqlite3_prepare_v2()],
** [sqlite3_prepare16()] and [sqlite3_prepare16_v2()].  ^at various
** points during the compilation process, as logic is being created
** to perform various actions, the authorizer callback is invoked to
** see if those actions are allowed.  ^the authorizer callback should
** return [sqlite_ok] to allow the action, [sqlite_ignore] to disallow the
** specific action but allow the sql statement to continue to be
** compiled, or [sqlite_deny] to cause the entire sql statement to be
** rejected with an error.  ^if the authorizer callback returns
** any value other than [sqlite_ignore], [sqlite_ok], or [sqlite_deny]
** then the [sqlite3_prepare_v2()] or equivalent call that triggered
** the authorizer will fail with an error message.
**
** when the callback returns [sqlite_ok], that means the operation
** requested is ok.  ^when the callback returns [sqlite_deny], the
** [sqlite3_prepare_v2()] or equivalent call that triggered the
** authorizer will fail with an error message explaining that
** access is denied. 
**
** ^the first parameter to the authorizer callback is a copy of the third
** parameter to the sqlite3_set_authorizer() interface. ^the second parameter
** to the callback is an integer [sqlite_copy | action code] that specifies
** the particular action to be authorized. ^the third through sixth parameters
** to the callback are zero-terminated strings that contain additional
** details about the action to be authorized.
**
** ^if the action code is [sqlite_read]
** and the callback returns [sqlite_ignore] then the
** [prepared statement] statement is constructed to substitute
** a null value in place of the table column that would have
** been read if [sqlite_ok] had been returned.  the [sqlite_ignore]
** return can be used to deny an untrusted user access to individual
** columns of a table.
** ^if the action code is [sqlite_delete] and the callback returns
** [sqlite_ignore] then the [delete] operation proceeds but the
** [truncate optimization] is disabled and all rows are deleted individually.
**
** an authorizer is used when [sqlite3_prepare | preparing]
** sql statements from an untrusted source, to ensure that the sql statements
** do not try to access data they are not allowed to see, or that they do not
** try to execute malicious statements that damage the database.  for
** example, an application may allow a user to enter arbitrary
** sql queries for evaluation by a database.  but the application does
** not want the user to be able to make arbitrary changes to the
** database.  an authorizer could then be put in place while the
** user-entered sql is being [sqlite3_prepare | prepared] that
** disallows everything except [select] statements.
**
** applications that need to process sql from untrusted sources
** might also consider lowering resource limits using [sqlite3_limit()]
** and limiting database size using the [max_page_count] [pragma]
** in addition to using an authorizer.
**
** ^(only a single authorizer can be in place on a database connection
** at a time.  each call to sqlite3_set_authorizer overrides the
** previous call.)^  ^disable the authorizer by installing a null callback.
** the authorizer is disabled by default.
**
** the authorizer callback must not do anything that will modify
** the database connection that invoked the authorizer callback.
** note that [sqlite3_prepare_v2()] and [sqlite3_step()] both modify their
** database connections for the meaning of "modify" in this paragraph.
**
** ^when [sqlite3_prepare_v2()] is used to prepare a statement, the
** statement might be re-prepared during [sqlite3_step()] due to a 
** schema change.  hence, the application should ensure that the
** correct authorizer callback remains in place during the [sqlite3_step()].
**
** ^note that the authorizer callback is invoked only during
** [sqlite3_prepare()] or its variants.  authorization is not
** performed during statement evaluation in [sqlite3_step()], unless
** as stated in the previous paragraph, sqlite3_step() invokes
** sqlite3_prepare_v2() to reprepare a statement after a schema change.
*/
sqlite_api int sqlite3_set_authorizer(
  sqlite3*,
  int (*xauth)(void*,int,const char*,const char*,const char*,const char*),
  void *puserdata
);

/*
** capi3ref: authorizer return codes
**
** the [sqlite3_set_authorizer | authorizer callback function] must
** return either [sqlite_ok] or one of these two constants in order
** to signal sqlite whether or not the action is permitted.  see the
** [sqlite3_set_authorizer | authorizer documentation] for additional
** information.
**
** note that sqlite_ignore is also used as a [conflict resolution mode]
** returned from the [sqlite3_vtab_on_conflict()] interface.
*/
#define sqlite_deny   1   /* abort the sql statement with an error */
#define sqlite_ignore 2   /* don't allow access, but don't generate an error */

/*
** capi3ref: authorizer action codes
**
** the [sqlite3_set_authorizer()] interface registers a callback function
** that is invoked to authorize certain sql statement actions.  the
** second parameter to the callback is an integer code that specifies
** what action is being authorized.  these are the integer action codes that
** the authorizer callback may be passed.
**
** these action code values signify what kind of operation is to be
** authorized.  the 3rd and 4th parameters to the authorization
** callback function will be parameters or null depending on which of these
** codes is used as the second parameter.  ^(the 5th parameter to the
** authorizer callback is the name of the database ("main", "temp",
** etc.) if applicable.)^  ^the 6th parameter to the authorizer callback
** is the name of the inner-most trigger or view that is responsible for
** the access attempt or null if this access attempt is directly from
** top-level sql code.
*/
/******************************************* 3rd ************ 4th ***********/
#define sqlite_create_index          1   /* index name      table name      */
#define sqlite_create_table          2   /* table name      null            */
#define sqlite_create_temp_index     3   /* index name      table name      */
#define sqlite_create_temp_table     4   /* table name      null            */
#define sqlite_create_temp_trigger   5   /* trigger name    table name      */
#define sqlite_create_temp_view      6   /* view name       null            */
#define sqlite_create_trigger        7   /* trigger name    table name      */
#define sqlite_create_view           8   /* view name       null            */
#define sqlite_delete                9   /* table name      null            */
#define sqlite_drop_index           10   /* index name      table name      */
#define sqlite_drop_table           11   /* table name      null            */
#define sqlite_drop_temp_index      12   /* index name      table name      */
#define sqlite_drop_temp_table      13   /* table name      null            */
#define sqlite_drop_temp_trigger    14   /* trigger name    table name      */
#define sqlite_drop_temp_view       15   /* view name       null            */
#define sqlite_drop_trigger         16   /* trigger name    table name      */
#define sqlite_drop_view            17   /* view name       null            */
#define sqlite_insert               18   /* table name      null            */
#define sqlite_pragma               19   /* pragma name     1st arg or null */
#define sqlite_read                 20   /* table name      column name     */
#define sqlite_select               21   /* null            null            */
#define sqlite_transaction          22   /* operation       null            */
#define sqlite_update               23   /* table name      column name     */
#define sqlite_attach               24   /* filename        null            */
#define sqlite_detach               25   /* database name   null            */
#define sqlite_alter_table          26   /* database name   table name      */
#define sqlite_reindex              27   /* index name      null            */
#define sqlite_analyze              28   /* table name      null            */
#define sqlite_create_vtable        29   /* table name      module name     */
#define sqlite_drop_vtable          30   /* table name      module name     */
#define sqlite_function             31   /* null            function name   */
#define sqlite_savepoint            32   /* operation       savepoint name  */
#define sqlite_copy                  0   /* no longer used */
#define sqlite_recursive            33   /* null            null            */

/*
** capi3ref: tracing and profiling functions
**
** these routines register callback functions that can be used for
** tracing and profiling the execution of sql statements.
**
** ^the callback function registered by sqlite3_trace() is invoked at
** various times when an sql statement is being run by [sqlite3_step()].
** ^the sqlite3_trace() callback is invoked with a utf-8 rendering of the
** sql statement text as the statement first begins executing.
** ^(additional sqlite3_trace() callbacks might occur
** as each triggered subprogram is entered.  the callbacks for triggers
** contain a utf-8 sql comment that identifies the trigger.)^
**
** the [sqlite_trace_size_limit] compile-time option can be used to limit
** the length of [bound parameter] expansion in the output of sqlite3_trace().
**
** ^the callback function registered by sqlite3_profile() is invoked
** as each sql statement finishes.  ^the profile callback contains
** the original statement text and an estimate of wall-clock time
** of how long that statement took to run.  ^the profile callback
** time is in units of nanoseconds, however the current implementation
** is only capable of millisecond resolution so the six least significant
** digits in the time are meaningless.  future versions of sqlite
** might provide greater resolution on the profiler callback.  the
** sqlite3_profile() function is considered experimental and is
** subject to change in future versions of sqlite.
*/
sqlite_api void *sqlite3_trace(sqlite3*, void(*xtrace)(void*,const char*), void*);
sqlite_api sqlite_experimental void *sqlite3_profile(sqlite3*,
   void(*xprofile)(void*,const char*,sqlite3_uint64), void*);

/*
** capi3ref: query progress callbacks
**
** ^the sqlite3_progress_handler(d,n,x,p) interface causes the callback
** function x to be invoked periodically during long running calls to
** [sqlite3_exec()], [sqlite3_step()] and [sqlite3_get_table()] for
** database connection d.  an example use for this
** interface is to keep a gui updated during a large query.
**
** ^the parameter p is passed through as the only parameter to the 
** callback function x.  ^the parameter n is the approximate number of 
** [virtual machine instructions] that are evaluated between successive
** invocations of the callback x.  ^if n is less than one then the progress
** handler is disabled.
**
** ^only a single progress handler may be defined at one time per
** [database connection]; setting a new progress handler cancels the
** old one.  ^setting parameter x to null disables the progress handler.
** ^the progress handler is also disabled by setting n to a value less
** than 1.
**
** ^if the progress callback returns non-zero, the operation is
** interrupted.  this feature can be used to implement a
** "cancel" button on a gui progress dialog box.
**
** the progress handler callback must not do anything that will modify
** the database connection that invoked the progress handler.
** note that [sqlite3_prepare_v2()] and [sqlite3_step()] both modify their
** database connections for the meaning of "modify" in this paragraph.
**
*/
sqlite_api void sqlite3_progress_handler(sqlite3*, int, int(*)(void*), void*);

/*
** capi3ref: opening a new database connection
**
** ^these routines open an sqlite database file as specified by the 
** filename argument. ^the filename argument is interpreted as utf-8 for
** sqlite3_open() and sqlite3_open_v2() and as utf-16 in the native byte
** order for sqlite3_open16(). ^(a [database connection] handle is usually
** returned in *ppdb, even if an error occurs.  the only exception is that
** if sqlite is unable to allocate memory to hold the [sqlite3] object,
** a null will be written into *ppdb instead of a pointer to the [sqlite3]
** object.)^ ^(if the database is opened (and/or created) successfully, then
** [sqlite_ok] is returned.  otherwise an [error code] is returned.)^ ^the
** [sqlite3_errmsg()] or [sqlite3_errmsg16()] routines can be used to obtain
** an english language description of the error following a failure of any
** of the sqlite3_open() routines.
**
** ^the default encoding will be utf-8 for databases created using
** sqlite3_open() or sqlite3_open_v2().  ^the default encoding for databases
** created using sqlite3_open16() will be utf-16 in the native byte order.
**
** whether or not an error occurs when it is opened, resources
** associated with the [database connection] handle should be released by
** passing it to [sqlite3_close()] when it is no longer required.
**
** the sqlite3_open_v2() interface works like sqlite3_open()
** except that it accepts two additional parameters for additional control
** over the new database connection.  ^(the flags parameter to
** sqlite3_open_v2() can take one of
** the following three values, optionally combined with the 
** [sqlite_open_nomutex], [sqlite_open_fullmutex], [sqlite_open_sharedcache],
** [sqlite_open_privatecache], and/or [sqlite_open_uri] flags:)^
**
** <dl>
** ^(<dt>[sqlite_open_readonly]</dt>
** <dd>the database is opened in read-only mode.  if the database does not
** already exist, an error is returned.</dd>)^
**
** ^(<dt>[sqlite_open_readwrite]</dt>
** <dd>the database is opened for reading and writing if possible, or reading
** only if the file is write protected by the operating system.  in either
** case the database must already exist, otherwise an error is returned.</dd>)^
**
** ^(<dt>[sqlite_open_readwrite] | [sqlite_open_create]</dt>
** <dd>the database is opened for reading and writing, and is created if
** it does not already exist. this is the behavior that is always used for
** sqlite3_open() and sqlite3_open16().</dd>)^
** </dl>
**
** if the 3rd parameter to sqlite3_open_v2() is not one of the
** combinations shown above optionally combined with other
** [sqlite_open_readonly | sqlite_open_* bits]
** then the behavior is undefined.
**
** ^if the [sqlite_open_nomutex] flag is set, then the database connection
** opens in the multi-thread [threading mode] as long as the single-thread
** mode has not been set at compile-time or start-time.  ^if the
** [sqlite_open_fullmutex] flag is set then the database connection opens
** in the serialized [threading mode] unless single-thread was
** previously selected at compile-time or start-time.
** ^the [sqlite_open_sharedcache] flag causes the database connection to be
** eligible to use [shared cache mode], regardless of whether or not shared
** cache is enabled using [sqlite3_enable_shared_cache()].  ^the
** [sqlite_open_privatecache] flag causes the database connection to not
** participate in [shared cache mode] even if it is enabled.
**
** ^the fourth parameter to sqlite3_open_v2() is the name of the
** [sqlite3_vfs] object that defines the operating system interface that
** the new database connection should use.  ^if the fourth parameter is
** a null pointer then the default [sqlite3_vfs] object is used.
**
** ^if the filename is ":memory:", then a private, temporary in-memory database
** is created for the connection.  ^this in-memory database will vanish when
** the database connection is closed.  future versions of sqlite might
** make use of additional special filenames that begin with the ":" character.
** it is recommended that when a database filename actually does begin with
** a ":" character you should prefix the filename with a pathname such as
** "./" to avoid ambiguity.
**
** ^if the filename is an empty string, then a private, temporary
** on-disk database will be created.  ^this private database will be
** automatically deleted as soon as the database connection is closed.
**
** [[uri filenames in sqlite3_open()]] <h3>uri filenames</h3>
**
** ^if [uri filename] interpretation is enabled, and the filename argument
** begins with "file:", then the filename is interpreted as a uri. ^uri
** filename interpretation is enabled if the [sqlite_open_uri] flag is
** set in the fourth argument to sqlite3_open_v2(), or if it has
** been enabled globally using the [sqlite_config_uri] option with the
** [sqlite3_config()] method or by the [sqlite_use_uri] compile-time option.
** as of sqlite version 3.7.7, uri filename interpretation is turned off
** by default, but future releases of sqlite might enable uri filename
** interpretation by default.  see "[uri filenames]" for additional
** information.
**
** uri filenames are parsed according to rfc 3986. ^if the uri contains an
** authority, then it must be either an empty string or the string 
** "localhost". ^if the authority is not an empty string or "localhost", an 
** error is returned to the caller. ^the fragment component of a uri, if 
** present, is ignored.
**
** ^sqlite uses the path component of the uri as the name of the disk file
** which contains the database. ^if the path begins with a '/' character, 
** then it is interpreted as an absolute path. ^if the path does not begin 
** with a '/' (meaning that the authority section is omitted from the uri)
** then the path is interpreted as a relative path. 
** ^(on windows, the first component of an absolute path 
** is a drive specification (e.g. "c:").)^
**
** [[core uri query parameters]]
** the query component of a uri may contain parameters that are interpreted
** either by sqlite itself, or by a [vfs | custom vfs implementation].
** sqlite and its built-in [vfses] interpret the
** following query parameters:
**
** <ul>
**   <li> <b>vfs</b>: ^the "vfs" parameter may be used to specify the name of
**     a vfs object that provides the operating system interface that should
**     be used to access the database file on disk. ^if this option is set to
**     an empty string the default vfs object is used. ^specifying an unknown
**     vfs is an error. ^if sqlite3_open_v2() is used and the vfs option is
**     present, then the vfs specified by the option takes precedence over
**     the value passed as the fourth parameter to sqlite3_open_v2().
**
**   <li> <b>mode</b>: ^(the mode parameter may be set to either "ro", "rw",
**     "rwc", or "memory". attempting to set it to any other value is
**     an error)^. 
**     ^if "ro" is specified, then the database is opened for read-only 
**     access, just as if the [sqlite_open_readonly] flag had been set in the 
**     third argument to sqlite3_open_v2(). ^if the mode option is set to 
**     "rw", then the database is opened for read-write (but not create) 
**     access, as if sqlite_open_readwrite (but not sqlite_open_create) had 
**     been set. ^value "rwc" is equivalent to setting both 
**     sqlite_open_readwrite and sqlite_open_create.  ^if the mode option is
**     set to "memory" then a pure [in-memory database] that never reads
**     or writes from disk is used. ^it is an error to specify a value for
**     the mode parameter that is less restrictive than that specified by
**     the flags passed in the third parameter to sqlite3_open_v2().
**
**   <li> <b>cache</b>: ^the cache parameter may be set to either "shared" or
**     "private". ^setting it to "shared" is equivalent to setting the
**     sqlite_open_sharedcache bit in the flags argument passed to
**     sqlite3_open_v2(). ^setting the cache parameter to "private" is 
**     equivalent to setting the sqlite_open_privatecache bit.
**     ^if sqlite3_open_v2() is used and the "cache" parameter is present in
**     a uri filename, its value overrides any behavior requested by setting
**     sqlite_open_privatecache or sqlite_open_sharedcache flag.
**
**  <li> <b>psow</b>: ^the psow parameter indicates whether or not the
**     [powersafe overwrite] property does or does not apply to the
**     storage media on which the database file resides.
**
**  <li> <b>nolock</b>: ^the nolock parameter is a boolean query parameter
**     which if set disables file locking in rollback journal modes.  this
**     is useful for accessing a database on a filesystem that does not
**     support locking.  caution:  database corruption might result if two
**     or more processes write to the same database and any one of those
**     processes uses nolock=1.
**
**  <li> <b>immutable</b>: ^the immutable parameter is a boolean query
**     parameter that indicates that the database file is stored on
**     read-only media.  ^when immutable is set, sqlite assumes that the
**     database file cannot be changed, even by a process with higher
**     privilege, and so the database is opened read-only and all locking
**     and change detection is disabled.  caution: setting the immutable
**     property on a database file that does in fact change can result
**     in incorrect query results and/or [sqlite_corrupt] errors.
**     see also: [sqlite_iocap_immutable].
**       
** </ul>
**
** ^specifying an unknown parameter in the query component of a uri is not an
** error.  future versions of sqlite might understand additional query
** parameters.  see "[query parameters with special meaning to sqlite]" for
** additional information.
**
** [[uri filename examples]] <h3>uri filename examples</h3>
**
** <table border="1" align=center cellpadding=5>
** <tr><th> uri filenames <th> results
** <tr><td> file:data.db <td> 
**          open the file "data.db" in the current directory.
** <tr><td> file:/home/fred/data.db<br>
**          file:///home/fred/data.db <br> 
**          file://localhost/home/fred/data.db <br> <td> 
**          open the database file "/home/fred/data.db".
** <tr><td> file://darkstar/home/fred/data.db <td> 
**          an error. "darkstar" is not a recognized authority.
** <tr><td style="white-space:nowrap"> 
**          file:///c:/documents%20and%20settings/fred/desktop/data.db
**     <td> windows only: open the file "data.db" on fred's desktop on drive
**          c:. note that the %20 escaping in this example is not strictly 
**          necessary - space characters can be used literally
**          in uri filenames.
** <tr><td> file:data.db?mode=ro&cache=private <td> 
**          open file "data.db" in the current directory for read-only access.
**          regardless of whether or not shared-cache mode is enabled by
**          default, use a private cache.
** <tr><td> file:/home/fred/data.db?vfs=unix-dotfile <td>
**          open file "/home/fred/data.db". use the special vfs "unix-dotfile"
**          that uses dot-files in place of posix advisory locking.
** <tr><td> file:data.db?mode=readonly <td> 
**          an error. "readonly" is not a valid option for the "mode" parameter.
** </table>
**
** ^uri hexadecimal escape sequences (%hh) are supported within the path and
** query components of a uri. a hexadecimal escape sequence consists of a
** percent sign - "%" - followed by exactly two hexadecimal digits 
** specifying an octet value. ^before the path or query components of a
** uri filename are interpreted, they are encoded using utf-8 and all 
** hexadecimal escape sequences replaced by a single byte containing the
** corresponding octet. if this process generates an invalid utf-8 encoding,
** the results are undefined.
**
** <b>note to windows users:</b>  the encoding used for the filename argument
** of sqlite3_open() and sqlite3_open_v2() must be utf-8, not whatever
** codepage is currently defined.  filenames containing international
** characters must be converted to utf-8 prior to passing them into
** sqlite3_open() or sqlite3_open_v2().
**
** <b>note to windows runtime users:</b>  the temporary directory must be set
** prior to calling sqlite3_open() or sqlite3_open_v2().  otherwise, various
** features that require the use of temporary files may fail.
**
** see also: [sqlite3_temp_directory]
*/
sqlite_api int sqlite3_open(
  const char *filename,   /* database filename (utf-8) */
  sqlite3 **ppdb          /* out: sqlite db handle */
);
sqlite_api int sqlite3_open16(
  const void *filename,   /* database filename (utf-16) */
  sqlite3 **ppdb          /* out: sqlite db handle */
);
sqlite_api int sqlite3_open_v2(
  const char *filename,   /* database filename (utf-8) */
  sqlite3 **ppdb,         /* out: sqlite db handle */
  int flags,              /* flags */
  const char *zvfs        /* name of vfs module to use */
);

/*
** capi3ref: obtain values for uri parameters
**
** these are utility routines, useful to vfs implementations, that check
** to see if a database file was a uri that contained a specific query 
** parameter, and if so obtains the value of that query parameter.
**
** if f is the database filename pointer passed into the xopen() method of 
** a vfs implementation when the flags parameter to xopen() has one or 
** more of the [sqlite_open_uri] or [sqlite_open_main_db] bits set and
** p is the name of the query parameter, then
** sqlite3_uri_parameter(f,p) returns the value of the p
** parameter if it exists or a null pointer if p does not appear as a 
** query parameter on f.  if p is a query parameter of f
** has no explicit value, then sqlite3_uri_parameter(f,p) returns
** a pointer to an empty string.
**
** the sqlite3_uri_boolean(f,p,b) routine assumes that p is a boolean
** parameter and returns true (1) or false (0) according to the value
** of p.  the sqlite3_uri_boolean(f,p,b) routine returns true (1) if the
** value of query parameter p is one of "yes", "true", or "on" in any
** case or if the value begins with a non-zero number.  the 
** sqlite3_uri_boolean(f,p,b) routines returns false (0) if the value of
** query parameter p is one of "no", "false", or "off" in any case or
** if the value begins with a numeric zero.  if p is not a query
** parameter on f or if the value of p is does not match any of the
** above, then sqlite3_uri_boolean(f,p,b) returns (b!=0).
**
** the sqlite3_uri_int64(f,p,d) routine converts the value of p into a
** 64-bit signed integer and returns that integer, or d if p does not
** exist.  if the value of p is something other than an integer, then
** zero is returned.
** 
** if f is a null pointer, then sqlite3_uri_parameter(f,p) returns null and
** sqlite3_uri_boolean(f,p,b) returns b.  if f is not a null pointer and
** is not a database file pathname pointer that sqlite passed into the xopen
** vfs method, then the behavior of this routine is undefined and probably
** undesirable.
*/
sqlite_api const char *sqlite3_uri_parameter(const char *zfilename, const char *zparam);
sqlite_api int sqlite3_uri_boolean(const char *zfile, const char *zparam, int bdefault);
sqlite_api sqlite3_int64 sqlite3_uri_int64(const char*, const char*, sqlite3_int64);


/*
** capi3ref: error codes and messages
**
** ^the sqlite3_errcode() interface returns the numeric [result code] or
** [extended result code] for the most recent failed sqlite3_* api call
** associated with a [database connection]. if a prior api call failed
** but the most recent api call succeeded, the return value from
** sqlite3_errcode() is undefined.  ^the sqlite3_extended_errcode()
** interface is the same except that it always returns the 
** [extended result code] even when extended result codes are
** disabled.
**
** ^the sqlite3_errmsg() and sqlite3_errmsg16() return english-language
** text that describes the error, as either utf-8 or utf-16 respectively.
** ^(memory to hold the error message string is managed internally.
** the application does not need to worry about freeing the result.
** however, the error string might be overwritten or deallocated by
** subsequent calls to other sqlite interface functions.)^
**
** ^the sqlite3_errstr() interface returns the english-language text
** that describes the [result code], as utf-8.
** ^(memory to hold the error message string is managed internally
** and must not be freed by the application)^.
**
** when the serialized [threading mode] is in use, it might be the
** case that a second error occurs on a separate thread in between
** the time of the first error and the call to these interfaces.
** when that happens, the second error will be reported since these
** interfaces always report the most recent result.  to avoid
** this, each thread can obtain exclusive use of the [database connection] d
** by invoking [sqlite3_mutex_enter]([sqlite3_db_mutex](d)) before beginning
** to use d and invoking [sqlite3_mutex_leave]([sqlite3_db_mutex](d)) after
** all calls to the interfaces listed here are completed.
**
** if an interface fails with sqlite_misuse, that means the interface
** was invoked incorrectly by the application.  in that case, the
** error code and message may or may not be set.
*/
sqlite_api int sqlite3_errcode(sqlite3 *db);
sqlite_api int sqlite3_extended_errcode(sqlite3 *db);
sqlite_api const char *sqlite3_errmsg(sqlite3*);
sqlite_api const void *sqlite3_errmsg16(sqlite3*);
sqlite_api const char *sqlite3_errstr(int);

/*
** capi3ref: sql statement object
** keywords: {prepared statement} {prepared statements}
**
** an instance of this object represents a single sql statement.
** this object is variously known as a "prepared statement" or a
** "compiled sql statement" or simply as a "statement".
**
** the life of a statement object goes something like this:
**
** <ol>
** <li> create the object using [sqlite3_prepare_v2()] or a related
**      function.
** <li> bind values to [host parameters] using the sqlite3_bind_*()
**      interfaces.
** <li> run the sql by calling [sqlite3_step()] one or more times.
** <li> reset the statement using [sqlite3_reset()] then go back
**      to step 2.  do this zero or more times.
** <li> destroy the object using [sqlite3_finalize()].
** </ol>
**
** refer to documentation on individual methods above for additional
** information.
*/
typedef struct sqlite3_stmt sqlite3_stmt;

/*
** capi3ref: run-time limits
**
** ^(this interface allows the size of various constructs to be limited
** on a connection by connection basis.  the first parameter is the
** [database connection] whose limit is to be set or queried.  the
** second parameter is one of the [limit categories] that define a
** class of constructs to be size limited.  the third parameter is the
** new limit for that construct.)^
**
** ^if the new limit is a negative number, the limit is unchanged.
** ^(for each limit category sqlite_limit_<i>name</i> there is a 
** [limits | hard upper bound]
** set at compile-time by a c preprocessor macro called
** [limits | sqlite_max_<i>name</i>].
** (the "_limit_" in the name is changed to "_max_".))^
** ^attempts to increase a limit above its hard upper bound are
** silently truncated to the hard upper bound.
**
** ^regardless of whether or not the limit was changed, the 
** [sqlite3_limit()] interface returns the prior value of the limit.
** ^hence, to find the current value of a limit without changing it,
** simply invoke this interface with the third parameter set to -1.
**
** run-time limits are intended for use in applications that manage
** both their own internal database and also databases that are controlled
** by untrusted external sources.  an example application might be a
** web browser that has its own databases for storing history and
** separate databases controlled by javascript applications downloaded
** off the internet.  the internal databases can be given the
** large, default limits.  databases managed by external sources can
** be given much smaller limits designed to prevent a denial of service
** attack.  developers might also want to use the [sqlite3_set_authorizer()]
** interface to further control untrusted sql.  the size of the database
** created by an untrusted script can be contained using the
** [max_page_count] [pragma].
**
** new run-time limit categories may be added in future releases.
*/
sqlite_api int sqlite3_limit(sqlite3*, int id, int newval);

/*
** capi3ref: run-time limit categories
** keywords: {limit category} {*limit categories}
**
** these constants define various performance limits
** that can be lowered at run-time using [sqlite3_limit()].
** the synopsis of the meanings of the various limits is shown below.
** additional information is available at [limits | limits in sqlite].
**
** <dl>
** [[sqlite_limit_length]] ^(<dt>sqlite_limit_length</dt>
** <dd>the maximum size of any string or blob or table row, in bytes.<dd>)^
**
** [[sqlite_limit_sql_length]] ^(<dt>sqlite_limit_sql_length</dt>
** <dd>the maximum length of an sql statement, in bytes.</dd>)^
**
** [[sqlite_limit_column]] ^(<dt>sqlite_limit_column</dt>
** <dd>the maximum number of columns in a table definition or in the
** result set of a [select] or the maximum number of columns in an index
** or in an order by or group by clause.</dd>)^
**
** [[sqlite_limit_expr_depth]] ^(<dt>sqlite_limit_expr_depth</dt>
** <dd>the maximum depth of the parse tree on any expression.</dd>)^
**
** [[sqlite_limit_compound_select]] ^(<dt>sqlite_limit_compound_select</dt>
** <dd>the maximum number of terms in a compound select statement.</dd>)^
**
** [[sqlite_limit_vdbe_op]] ^(<dt>sqlite_limit_vdbe_op</dt>
** <dd>the maximum number of instructions in a virtual machine program
** used to implement an sql statement.  this limit is not currently
** enforced, though that might be added in some future release of
** sqlite.</dd>)^
**
** [[sqlite_limit_function_arg]] ^(<dt>sqlite_limit_function_arg</dt>
** <dd>the maximum number of arguments on a function.</dd>)^
**
** [[sqlite_limit_attached]] ^(<dt>sqlite_limit_attached</dt>
** <dd>the maximum number of [attach | attached databases].)^</dd>
**
** [[sqlite_limit_like_pattern_length]]
** ^(<dt>sqlite_limit_like_pattern_length</dt>
** <dd>the maximum length of the pattern argument to the [like] or
** [glob] operators.</dd>)^
**
** [[sqlite_limit_variable_number]]
** ^(<dt>sqlite_limit_variable_number</dt>
** <dd>the maximum index number of any [parameter] in an sql statement.)^
**
** [[sqlite_limit_trigger_depth]] ^(<dt>sqlite_limit_trigger_depth</dt>
** <dd>the maximum depth of recursion for triggers.</dd>)^
**
** [[sqlite_limit_worker_threads]] ^(<dt>sqlite_limit_worker_threads</dt>
** <dd>the maximum number of auxiliary worker threads that a single
** [prepared statement] may start.</dd>)^
** </dl>
*/
#define sqlite_limit_length                    0
#define sqlite_limit_sql_length                1
#define sqlite_limit_column                    2
#define sqlite_limit_expr_depth                3
#define sqlite_limit_compound_select           4
#define sqlite_limit_vdbe_op                   5
#define sqlite_limit_function_arg              6
#define sqlite_limit_attached                  7
#define sqlite_limit_like_pattern_length       8
#define sqlite_limit_variable_number           9
#define sqlite_limit_trigger_depth            10
#define sqlite_limit_worker_threads           11

/*
** capi3ref: compiling an sql statement
** keywords: {sql statement compiler}
**
** to execute an sql query, it must first be compiled into a byte-code
** program using one of these routines.
**
** the first argument, "db", is a [database connection] obtained from a
** prior successful call to [sqlite3_open()], [sqlite3_open_v2()] or
** [sqlite3_open16()].  the database connection must not have been closed.
**
** the second argument, "zsql", is the statement to be compiled, encoded
** as either utf-8 or utf-16.  the sqlite3_prepare() and sqlite3_prepare_v2()
** interfaces use utf-8, and sqlite3_prepare16() and sqlite3_prepare16_v2()
** use utf-16.
**
** ^if the nbyte argument is less than zero, then zsql is read up to the
** first zero terminator. ^if nbyte is non-negative, then it is the maximum
** number of  bytes read from zsql.  ^when nbyte is non-negative, the
** zsql string ends at either the first '\000' or '\u0000' character or
** the nbyte-th byte, whichever comes first. if the caller knows
** that the supplied string is nul-terminated, then there is a small
** performance advantage to be gained by passing an nbyte parameter that
** is equal to the number of bytes in the input string <i>including</i>
** the nul-terminator bytes as this saves sqlite from having to
** make a copy of the input string.
**
** ^if pztail is not null then *pztail is made to point to the first byte
** past the end of the first sql statement in zsql.  these routines only
** compile the first statement in zsql, so *pztail is left pointing to
** what remains uncompiled.
**
** ^*ppstmt is left pointing to a compiled [prepared statement] that can be
** executed using [sqlite3_step()].  ^if there is an error, *ppstmt is set
** to null.  ^if the input text contains no sql (if the input is an empty
** string or a comment) then *ppstmt is set to null.
** the calling procedure is responsible for deleting the compiled
** sql statement using [sqlite3_finalize()] after it has finished with it.
** ppstmt may not be null.
**
** ^on success, the sqlite3_prepare() family of routines return [sqlite_ok];
** otherwise an [error code] is returned.
**
** the sqlite3_prepare_v2() and sqlite3_prepare16_v2() interfaces are
** recommended for all new programs. the two older interfaces are retained
** for backwards compatibility, but their use is discouraged.
** ^in the "v2" interfaces, the prepared statement
** that is returned (the [sqlite3_stmt] object) contains a copy of the
** original sql text. this causes the [sqlite3_step()] interface to
** behave differently in three ways:
**
** <ol>
** <li>
** ^if the database schema changes, instead of returning [sqlite_schema] as it
** always used to do, [sqlite3_step()] will automatically recompile the sql
** statement and try to run it again. as many as [sqlite_max_schema_retry]
** retries will occur before sqlite3_step() gives up and returns an error.
** </li>
**
** <li>
** ^when an error occurs, [sqlite3_step()] will return one of the detailed
** [error codes] or [extended error codes].  ^the legacy behavior was that
** [sqlite3_step()] would only return a generic [sqlite_error] result code
** and the application would have to make a second call to [sqlite3_reset()]
** in order to find the underlying cause of the problem. with the "v2" prepare
** interfaces, the underlying reason for the error is returned immediately.
** </li>
**
** <li>
** ^if the specific value bound to [parameter | host parameter] in the 
** where clause might influence the choice of query plan for a statement,
** then the statement will be automatically recompiled, as if there had been 
** a schema change, on the first  [sqlite3_step()] call following any change
** to the [sqlite3_bind_text | bindings] of that [parameter]. 
** ^the specific value of where-clause [parameter] might influence the 
** choice of query plan if the parameter is the left-hand side of a [like]
** or [glob] operator or if the parameter is compared to an indexed column
** and the [sqlite_enable_stat3] compile-time option is enabled.
** </li>
** </ol>
*/
sqlite_api int sqlite3_prepare(
  sqlite3 *db,            /* database handle */
  const char *zsql,       /* sql statement, utf-8 encoded */
  int nbyte,              /* maximum length of zsql in bytes. */
  sqlite3_stmt **ppstmt,  /* out: statement handle */
  const char **pztail     /* out: pointer to unused portion of zsql */
);
sqlite_api int sqlite3_prepare_v2(
  sqlite3 *db,            /* database handle */
  const char *zsql,       /* sql statement, utf-8 encoded */
  int nbyte,              /* maximum length of zsql in bytes. */
  sqlite3_stmt **ppstmt,  /* out: statement handle */
  const char **pztail     /* out: pointer to unused portion of zsql */
);
sqlite_api int sqlite3_prepare16(
  sqlite3 *db,            /* database handle */
  const void *zsql,       /* sql statement, utf-16 encoded */
  int nbyte,              /* maximum length of zsql in bytes. */
  sqlite3_stmt **ppstmt,  /* out: statement handle */
  const void **pztail     /* out: pointer to unused portion of zsql */
);
sqlite_api int sqlite3_prepare16_v2(
  sqlite3 *db,            /* database handle */
  const void *zsql,       /* sql statement, utf-16 encoded */
  int nbyte,              /* maximum length of zsql in bytes. */
  sqlite3_stmt **ppstmt,  /* out: statement handle */
  const void **pztail     /* out: pointer to unused portion of zsql */
);

/*
** capi3ref: retrieving statement sql
**
** ^this interface can be used to retrieve a saved copy of the original
** sql text used to create a [prepared statement] if that statement was
** compiled using either [sqlite3_prepare_v2()] or [sqlite3_prepare16_v2()].
*/
sqlite_api const char *sqlite3_sql(sqlite3_stmt *pstmt);

/*
** capi3ref: determine if an sql statement writes the database
**
** ^the sqlite3_stmt_readonly(x) interface returns true (non-zero) if
** and only if the [prepared statement] x makes no direct changes to
** the content of the database file.
**
** note that [application-defined sql functions] or
** [virtual tables] might change the database indirectly as a side effect.  
** ^(for example, if an application defines a function "eval()" that 
** calls [sqlite3_exec()], then the following sql statement would
** change the database file through side-effects:
**
** <blockquote><pre>
**    select eval('delete from t1') from t2;
** </pre></blockquote>
**
** but because the [select] statement does not change the database file
** directly, sqlite3_stmt_readonly() would still return true.)^
**
** ^transaction control statements such as [begin], [commit], [rollback],
** [savepoint], and [release] cause sqlite3_stmt_readonly() to return true,
** since the statements themselves do not actually modify the database but
** rather they control the timing of when other statements modify the 
** database.  ^the [attach] and [detach] statements also cause
** sqlite3_stmt_readonly() to return true since, while those statements
** change the configuration of a database connection, they do not make 
** changes to the content of the database files on disk.
*/
sqlite_api int sqlite3_stmt_readonly(sqlite3_stmt *pstmt);

/*
** capi3ref: determine if a prepared statement has been reset
**
** ^the sqlite3_stmt_busy(s) interface returns true (non-zero) if the
** [prepared statement] s has been stepped at least once using 
** [sqlite3_step(s)] but has not run to completion and/or has not 
** been reset using [sqlite3_reset(s)].  ^the sqlite3_stmt_busy(s)
** interface returns false if s is a null pointer.  if s is not a 
** null pointer and is not a pointer to a valid [prepared statement]
** object, then the behavior is undefined and probably undesirable.
**
** this interface can be used in combination [sqlite3_next_stmt()]
** to locate all prepared statements associated with a database 
** connection that are in need of being reset.  this can be used,
** for example, in diagnostic routines to search for prepared 
** statements that are holding a transaction open.
*/
sqlite_api int sqlite3_stmt_busy(sqlite3_stmt*);

/*
** capi3ref: dynamically typed value object
** keywords: {protected sqlite3_value} {unprotected sqlite3_value}
**
** sqlite uses the sqlite3_value object to represent all values
** that can be stored in a database table. sqlite uses dynamic typing
** for the values it stores.  ^values stored in sqlite3_value objects
** can be integers, floating point values, strings, blobs, or null.
**
** an sqlite3_value object may be either "protected" or "unprotected".
** some interfaces require a protected sqlite3_value.  other interfaces
** will accept either a protected or an unprotected sqlite3_value.
** every interface that accepts sqlite3_value arguments specifies
** whether or not it requires a protected sqlite3_value.
**
** the terms "protected" and "unprotected" refer to whether or not
** a mutex is held.  an internal mutex is held for a protected
** sqlite3_value object but no mutex is held for an unprotected
** sqlite3_value object.  if sqlite is compiled to be single-threaded
** (with [sqlite_threadsafe=0] and with [sqlite3_threadsafe()] returning 0)
** or if sqlite is run in one of reduced mutex modes 
** [sqlite_config_singlethread] or [sqlite_config_multithread]
** then there is no distinction between protected and unprotected
** sqlite3_value objects and they can be used interchangeably.  however,
** for maximum code portability it is recommended that applications
** still make the distinction between protected and unprotected
** sqlite3_value objects even when not strictly required.
**
** ^the sqlite3_value objects that are passed as parameters into the
** implementation of [application-defined sql functions] are protected.
** ^the sqlite3_value object returned by
** [sqlite3_column_value()] is unprotected.
** unprotected sqlite3_value objects may only be used with
** [sqlite3_result_value()] and [sqlite3_bind_value()].
** the [sqlite3_value_blob | sqlite3_value_type()] family of
** interfaces require protected sqlite3_value objects.
*/
typedef struct mem sqlite3_value;

/*
** capi3ref: sql function context object
**
** the context in which an sql function executes is stored in an
** sqlite3_context object.  ^a pointer to an sqlite3_context object
** is always first parameter to [application-defined sql functions].
** the application-defined sql function implementation will pass this
** pointer through into calls to [sqlite3_result_int | sqlite3_result()],
** [sqlite3_aggregate_context()], [sqlite3_user_data()],
** [sqlite3_context_db_handle()], [sqlite3_get_auxdata()],
** and/or [sqlite3_set_auxdata()].
*/
typedef struct sqlite3_context sqlite3_context;

/*
** capi3ref: binding values to prepared statements
** keywords: {host parameter} {host parameters} {host parameter name}
** keywords: {sql parameter} {sql parameters} {parameter binding}
**
** ^(in the sql statement text input to [sqlite3_prepare_v2()] and its variants,
** literals may be replaced by a [parameter] that matches one of following
** templates:
**
** <ul>
** <li>  ?
** <li>  ?nnn
** <li>  :vvv
** <li>  @vvv
** <li>  $vvv
** </ul>
**
** in the templates above, nnn represents an integer literal,
** and vvv represents an alphanumeric identifier.)^  ^the values of these
** parameters (also called "host parameter names" or "sql parameters")
** can be set using the sqlite3_bind_*() routines defined here.
**
** ^the first argument to the sqlite3_bind_*() routines is always
** a pointer to the [sqlite3_stmt] object returned from
** [sqlite3_prepare_v2()] or its variants.
**
** ^the second argument is the index of the sql parameter to be set.
** ^the leftmost sql parameter has an index of 1.  ^when the same named
** sql parameter is used more than once, second and subsequent
** occurrences have the same index as the first occurrence.
** ^the index for named parameters can be looked up using the
** [sqlite3_bind_parameter_index()] api if desired.  ^the index
** for "?nnn" parameters is the value of nnn.
** ^the nnn value must be between 1 and the [sqlite3_limit()]
** parameter [sqlite_limit_variable_number] (default value: 999).
**
** ^the third argument is the value to bind to the parameter.
** ^if the third parameter to sqlite3_bind_text() or sqlite3_bind_text16()
** or sqlite3_bind_blob() is a null pointer then the fourth parameter
** is ignored and the end result is the same as sqlite3_bind_null().
**
** ^(in those routines that have a fourth argument, its value is the
** number of bytes in the parameter.  to be clear: the value is the
** number of <u>bytes</u> in the value, not the number of characters.)^
** ^if the fourth parameter to sqlite3_bind_text() or sqlite3_bind_text16()
** is negative, then the length of the string is
** the number of bytes up to the first zero terminator.
** if the fourth parameter to sqlite3_bind_blob() is negative, then
** the behavior is undefined.
** if a non-negative fourth parameter is provided to sqlite3_bind_text()
** or sqlite3_bind_text16() or sqlite3_bind_text64() then
** that parameter must be the byte offset
** where the nul terminator would occur assuming the string were nul
** terminated.  if any nul characters occur at byte offsets less than 
** the value of the fourth parameter then the resulting string value will
** contain embedded nuls.  the result of expressions involving strings
** with embedded nuls is undefined.
**
** ^the fifth argument to the blob and string binding interfaces
** is a destructor used to dispose of the blob or
** string after sqlite has finished with it.  ^the destructor is called
** to dispose of the blob or string even if the call to bind api fails.
** ^if the fifth argument is
** the special value [sqlite_static], then sqlite assumes that the
** information is in static, unmanaged space and does not need to be freed.
** ^if the fifth argument has the value [sqlite_transient], then
** sqlite makes its own private copy of the data immediately, before
** the sqlite3_bind_*() routine returns.
**
** ^the sixth argument to sqlite3_bind_text64() must be one of
** [sqlite_utf8], [sqlite_utf16], [sqlite_utf16be], or [sqlite_utf16le]
** to specify the encoding of the text in the third parameter.  if
** the sixth argument to sqlite3_bind_text64() is not one of the
** allowed values shown above, or if the text encoding is different
** from the encoding specified by the sixth parameter, then the behavior
** is undefined.
**
** ^the sqlite3_bind_zeroblob() routine binds a blob of length n that
** is filled with zeroes.  ^a zeroblob uses a fixed amount of memory
** (just an integer to hold its size) while it is being processed.
** zeroblobs are intended to serve as placeholders for blobs whose
** content is later written using
** [sqlite3_blob_open | incremental blob i/o] routines.
** ^a negative value for the zeroblob results in a zero-length blob.
**
** ^if any of the sqlite3_bind_*() routines are called with a null pointer
** for the [prepared statement] or with a prepared statement for which
** [sqlite3_step()] has been called more recently than [sqlite3_reset()],
** then the call will return [sqlite_misuse].  if any sqlite3_bind_()
** routine is passed a [prepared statement] that has been finalized, the
** result is undefined and probably harmful.
**
** ^bindings are not cleared by the [sqlite3_reset()] routine.
** ^unbound parameters are interpreted as null.
**
** ^the sqlite3_bind_* routines return [sqlite_ok] on success or an
** [error code] if anything goes wrong.
** ^[sqlite_toobig] might be returned if the size of a string or blob
** exceeds limits imposed by [sqlite3_limit]([sqlite_limit_length]) or
** [sqlite_max_length].
** ^[sqlite_range] is returned if the parameter
** index is out of range.  ^[sqlite_nomem] is returned if malloc() fails.
**
** see also: [sqlite3_bind_parameter_count()],
** [sqlite3_bind_parameter_name()], and [sqlite3_bind_parameter_index()].
*/
sqlite_api int sqlite3_bind_blob(sqlite3_stmt*, int, const void*, int n, void(*)(void*));
sqlite_api int sqlite3_bind_blob64(sqlite3_stmt*, int, const void*, sqlite3_uint64,
                        void(*)(void*));
sqlite_api int sqlite3_bind_double(sqlite3_stmt*, int, double);
sqlite_api int sqlite3_bind_int(sqlite3_stmt*, int, int);
sqlite_api int sqlite3_bind_int64(sqlite3_stmt*, int, sqlite3_int64);
sqlite_api int sqlite3_bind_null(sqlite3_stmt*, int);
sqlite_api int sqlite3_bind_text(sqlite3_stmt*,int,const char*,int,void(*)(void*));
sqlite_api int sqlite3_bind_text16(sqlite3_stmt*, int, const void*, int, void(*)(void*));
sqlite_api int sqlite3_bind_text64(sqlite3_stmt*, int, const char*, sqlite3_uint64,
                         void(*)(void*), unsigned char encoding);
sqlite_api int sqlite3_bind_value(sqlite3_stmt*, int, const sqlite3_value*);
sqlite_api int sqlite3_bind_zeroblob(sqlite3_stmt*, int, int n);

/*
** capi3ref: number of sql parameters
**
** ^this routine can be used to find the number of [sql parameters]
** in a [prepared statement].  sql parameters are tokens of the
** form "?", "?nnn", ":aaa", "$aaa", or "@aaa" that serve as
** placeholders for values that are [sqlite3_bind_blob | bound]
** to the parameters at a later time.
**
** ^(this routine actually returns the index of the largest (rightmost)
** parameter. for all forms except ?nnn, this will correspond to the
** number of unique parameters.  if parameters of the ?nnn form are used,
** there may be gaps in the list.)^
**
** see also: [sqlite3_bind_blob|sqlite3_bind()],
** [sqlite3_bind_parameter_name()], and
** [sqlite3_bind_parameter_index()].
*/
sqlite_api int sqlite3_bind_parameter_count(sqlite3_stmt*);

/*
** capi3ref: name of a host parameter
**
** ^the sqlite3_bind_parameter_name(p,n) interface returns
** the name of the n-th [sql parameter] in the [prepared statement] p.
** ^(sql parameters of the form "?nnn" or ":aaa" or "@aaa" or "$aaa"
** have a name which is the string "?nnn" or ":aaa" or "@aaa" or "$aaa"
** respectively.
** in other words, the initial ":" or "$" or "@" or "?"
** is included as part of the name.)^
** ^parameters of the form "?" without a following integer have no name
** and are referred to as "nameless" or "anonymous parameters".
**
** ^the first host parameter has an index of 1, not 0.
**
** ^if the value n is out of range or if the n-th parameter is
** nameless, then null is returned.  ^the returned string is
** always in utf-8 encoding even if the named parameter was
** originally specified as utf-16 in [sqlite3_prepare16()] or
** [sqlite3_prepare16_v2()].
**
** see also: [sqlite3_bind_blob|sqlite3_bind()],
** [sqlite3_bind_parameter_count()], and
** [sqlite3_bind_parameter_index()].
*/
sqlite_api const char *sqlite3_bind_parameter_name(sqlite3_stmt*, int);

/*
** capi3ref: index of a parameter with a given name
**
** ^return the index of an sql parameter given its name.  ^the
** index value returned is suitable for use as the second
** parameter to [sqlite3_bind_blob|sqlite3_bind()].  ^a zero
** is returned if no matching parameter is found.  ^the parameter
** name must be given in utf-8 even if the original statement
** was prepared from utf-16 text using [sqlite3_prepare16_v2()].
**
** see also: [sqlite3_bind_blob|sqlite3_bind()],
** [sqlite3_bind_parameter_count()], and
** [sqlite3_bind_parameter_index()].
*/
sqlite_api int sqlite3_bind_parameter_index(sqlite3_stmt*, const char *zname);

/*
** capi3ref: reset all bindings on a prepared statement
**
** ^contrary to the intuition of many, [sqlite3_reset()] does not reset
** the [sqlite3_bind_blob | bindings] on a [prepared statement].
** ^use this routine to reset all host parameters to null.
*/
sqlite_api int sqlite3_clear_bindings(sqlite3_stmt*);

/*
** capi3ref: number of columns in a result set
**
** ^return the number of columns in the result set returned by the
** [prepared statement]. ^this routine returns 0 if pstmt is an sql
** statement that does not return data (for example an [update]).
**
** see also: [sqlite3_data_count()]
*/
sqlite_api int sqlite3_column_count(sqlite3_stmt *pstmt);

/*
** capi3ref: column names in a result set
**
** ^these routines return the name assigned to a particular column
** in the result set of a [select] statement.  ^the sqlite3_column_name()
** interface returns a pointer to a zero-terminated utf-8 string
** and sqlite3_column_name16() returns a pointer to a zero-terminated
** utf-16 string.  ^the first parameter is the [prepared statement]
** that implements the [select] statement. ^the second parameter is the
** column number.  ^the leftmost column is number 0.
**
** ^the returned string pointer is valid until either the [prepared statement]
** is destroyed by [sqlite3_finalize()] or until the statement is automatically
** reprepared by the first call to [sqlite3_step()] for a particular run
** or until the next call to
** sqlite3_column_name() or sqlite3_column_name16() on the same column.
**
** ^if sqlite3_malloc() fails during the processing of either routine
** (for example during a conversion from utf-8 to utf-16) then a
** null pointer is returned.
**
** ^the name of a result column is the value of the "as" clause for
** that column, if there is an as clause.  if there is no as clause
** then the name of the column is unspecified and may change from
** one release of sqlite to the next.
*/
sqlite_api const char *sqlite3_column_name(sqlite3_stmt*, int n);
sqlite_api const void *sqlite3_column_name16(sqlite3_stmt*, int n);

/*
** capi3ref: source of data in a query result
**
** ^these routines provide a means to determine the database, table, and
** table column that is the origin of a particular result column in
** [select] statement.
** ^the name of the database or table or column can be returned as
** either a utf-8 or utf-16 string.  ^the _database_ routines return
** the database name, the _table_ routines return the table name, and
** the origin_ routines return the column name.
** ^the returned string is valid until the [prepared statement] is destroyed
** using [sqlite3_finalize()] or until the statement is automatically
** reprepared by the first call to [sqlite3_step()] for a particular run
** or until the same information is requested
** again in a different encoding.
**
** ^the names returned are the original un-aliased names of the
** database, table, and column.
**
** ^the first argument to these interfaces is a [prepared statement].
** ^these functions return information about the nth result column returned by
** the statement, where n is the second function argument.
** ^the left-most column is column 0 for these routines.
**
** ^if the nth column returned by the statement is an expression or
** subquery and is not a column value, then all of these functions return
** null.  ^these routine might also return null if a memory allocation error
** occurs.  ^otherwise, they return the name of the attached database, table,
** or column that query result column was extracted from.
**
** ^as with all other sqlite apis, those whose names end with "16" return
** utf-16 encoded strings and the other functions return utf-8.
**
** ^these apis are only available if the library was compiled with the
** [sqlite_enable_column_metadata] c-preprocessor symbol.
**
** if two or more threads call one or more of these routines against the same
** prepared statement and column at the same time then the results are
** undefined.
**
** if two or more threads call one or more
** [sqlite3_column_database_name | column metadata interfaces]
** for the same [prepared statement] and result column
** at the same time then the results are undefined.
*/
sqlite_api const char *sqlite3_column_database_name(sqlite3_stmt*,int);
sqlite_api const void *sqlite3_column_database_name16(sqlite3_stmt*,int);
sqlite_api const char *sqlite3_column_table_name(sqlite3_stmt*,int);
sqlite_api const void *sqlite3_column_table_name16(sqlite3_stmt*,int);
sqlite_api const char *sqlite3_column_origin_name(sqlite3_stmt*,int);
sqlite_api const void *sqlite3_column_origin_name16(sqlite3_stmt*,int);

/*
** capi3ref: declared datatype of a query result
**
** ^(the first parameter is a [prepared statement].
** if this statement is a [select] statement and the nth column of the
** returned result set of that [select] is a table column (not an
** expression or subquery) then the declared type of the table
** column is returned.)^  ^if the nth column of the result set is an
** expression or subquery, then a null pointer is returned.
** ^the returned string is always utf-8 encoded.
**
** ^(for example, given the database schema:
**
** create table t1(c1 variant);
**
** and the following statement to be compiled:
**
** select c1 + 1, c1 from t1;
**
** this routine would return the string "variant" for the second result
** column (i==1), and a null pointer for the first result column (i==0).)^
**
** ^sqlite uses dynamic run-time typing.  ^so just because a column
** is declared to contain a particular type does not mean that the
** data stored in that column is of the declared type.  sqlite is
** strongly typed, but the typing is dynamic not static.  ^type
** is associated with individual values, not with the containers
** used to hold those values.
*/
sqlite_api const char *sqlite3_column_decltype(sqlite3_stmt*,int);
sqlite_api const void *sqlite3_column_decltype16(sqlite3_stmt*,int);

/*
** capi3ref: evaluate an sql statement
**
** after a [prepared statement] has been prepared using either
** [sqlite3_prepare_v2()] or [sqlite3_prepare16_v2()] or one of the legacy
** interfaces [sqlite3_prepare()] or [sqlite3_prepare16()], this function
** must be called one or more times to evaluate the statement.
**
** the details of the behavior of the sqlite3_step() interface depend
** on whether the statement was prepared using the newer "v2" interface
** [sqlite3_prepare_v2()] and [sqlite3_prepare16_v2()] or the older legacy
** interface [sqlite3_prepare()] and [sqlite3_prepare16()].  the use of the
** new "v2" interface is recommended for new applications but the legacy
** interface will continue to be supported.
**
** ^in the legacy interface, the return value will be either [sqlite_busy],
** [sqlite_done], [sqlite_row], [sqlite_error], or [sqlite_misuse].
** ^with the "v2" interface, any of the other [result codes] or
** [extended result codes] might be returned as well.
**
** ^[sqlite_busy] means that the database engine was unable to acquire the
** database locks it needs to do its job.  ^if the statement is a [commit]
** or occurs outside of an explicit transaction, then you can retry the
** statement.  if the statement is not a [commit] and occurs within an
** explicit transaction then you should rollback the transaction before
** continuing.
**
** ^[sqlite_done] means that the statement has finished executing
** successfully.  sqlite3_step() should not be called again on this virtual
** machine without first calling [sqlite3_reset()] to reset the virtual
** machine back to its initial state.
**
** ^if the sql statement being executed returns any data, then [sqlite_row]
** is returned each time a new row of data is ready for processing by the
** caller. the values may be accessed using the [column access functions].
** sqlite3_step() is called again to retrieve the next row of data.
**
** ^[sqlite_error] means that a run-time error (such as a constraint
** violation) has occurred.  sqlite3_step() should not be called again on
** the vm. more information may be found by calling [sqlite3_errmsg()].
** ^with the legacy interface, a more specific error code (for example,
** [sqlite_interrupt], [sqlite_schema], [sqlite_corrupt], and so forth)
** can be obtained by calling [sqlite3_reset()] on the
** [prepared statement].  ^in the "v2" interface,
** the more specific error code is returned directly by sqlite3_step().
**
** [sqlite_misuse] means that the this routine was called inappropriately.
** perhaps it was called on a [prepared statement] that has
** already been [sqlite3_finalize | finalized] or on one that had
** previously returned [sqlite_error] or [sqlite_done].  or it could
** be the case that the same database connection is being used by two or
** more threads at the same moment in time.
**
** for all versions of sqlite up to and including 3.6.23.1, a call to
** [sqlite3_reset()] was required after sqlite3_step() returned anything
** other than [sqlite_row] before any subsequent invocation of
** sqlite3_step().  failure to reset the prepared statement using 
** [sqlite3_reset()] would result in an [sqlite_misuse] return from
** sqlite3_step().  but after version 3.6.23.1, sqlite3_step() began
** calling [sqlite3_reset()] automatically in this circumstance rather
** than returning [sqlite_misuse].  this is not considered a compatibility
** break because any application that ever receives an sqlite_misuse error
** is broken by definition.  the [sqlite_omit_autoreset] compile-time option
** can be used to restore the legacy behavior.
**
** <b>goofy interface alert:</b> in the legacy interface, the sqlite3_step()
** api always returns a generic error code, [sqlite_error], following any
** error other than [sqlite_busy] and [sqlite_misuse].  you must call
** [sqlite3_reset()] or [sqlite3_finalize()] in order to find one of the
** specific [error codes] that better describes the error.
** we admit that this is a goofy design.  the problem has been fixed
** with the "v2" interface.  if you prepare all of your sql statements
** using either [sqlite3_prepare_v2()] or [sqlite3_prepare16_v2()] instead
** of the legacy [sqlite3_prepare()] and [sqlite3_prepare16()] interfaces,
** then the more specific [error codes] are returned directly
** by sqlite3_step().  the use of the "v2" interface is recommended.
*/
sqlite_api int sqlite3_step(sqlite3_stmt*);

/*
** capi3ref: number of columns in a result set
**
** ^the sqlite3_data_count(p) interface returns the number of columns in the
** current row of the result set of [prepared statement] p.
** ^if prepared statement p does not have results ready to return
** (via calls to the [sqlite3_column_int | sqlite3_column_*()] of
** interfaces) then sqlite3_data_count(p) returns 0.
** ^the sqlite3_data_count(p) routine also returns 0 if p is a null pointer.
** ^the sqlite3_data_count(p) routine returns 0 if the previous call to
** [sqlite3_step](p) returned [sqlite_done].  ^the sqlite3_data_count(p)
** will return non-zero if previous call to [sqlite3_step](p) returned
** [sqlite_row], except in the case of the [pragma incremental_vacuum]
** where it always returns zero since each step of that multi-step
** pragma returns 0 columns of data.
**
** see also: [sqlite3_column_count()]
*/
sqlite_api int sqlite3_data_count(sqlite3_stmt *pstmt);

/*
** capi3ref: fundamental datatypes
** keywords: sqlite_text
**
** ^(every value in sqlite has one of five fundamental datatypes:
**
** <ul>
** <li> 64-bit signed integer
** <li> 64-bit ieee floating point number
** <li> string
** <li> blob
** <li> null
** </ul>)^
**
** these constants are codes for each of those types.
**
** note that the sqlite_text constant was also used in sqlite version 2
** for a completely different meaning.  software that links against both
** sqlite version 2 and sqlite version 3 should use sqlite3_text, not
** sqlite_text.
*/
#define sqlite_integer  1
#define sqlite_float    2
#define sqlite_blob     4
#define sqlite_null     5
#ifdef sqlite_text
# undef sqlite_text
#else
# define sqlite_text     3
#endif
#define sqlite3_text     3

/*
** capi3ref: result values from a query
** keywords: {column access functions}
**
** these routines form the "result set" interface.
**
** ^these routines return information about a single column of the current
** result row of a query.  ^in every case the first argument is a pointer
** to the [prepared statement] that is being evaluated (the [sqlite3_stmt*]
** that was returned from [sqlite3_prepare_v2()] or one of its variants)
** and the second argument is the index of the column for which information
** should be returned. ^the leftmost column of the result set has the index 0.
** ^the number of columns in the result can be determined using
** [sqlite3_column_count()].
**
** if the sql statement does not currently point to a valid row, or if the
** column index is out of range, the result is undefined.
** these routines may only be called when the most recent call to
** [sqlite3_step()] has returned [sqlite_row] and neither
** [sqlite3_reset()] nor [sqlite3_finalize()] have been called subsequently.
** if any of these routines are called after [sqlite3_reset()] or
** [sqlite3_finalize()] or after [sqlite3_step()] has returned
** something other than [sqlite_row], the results are undefined.
** if [sqlite3_step()] or [sqlite3_reset()] or [sqlite3_finalize()]
** are called from a different thread while any of these routines
** are pending, then the results are undefined.
**
** ^the sqlite3_column_type() routine returns the
** [sqlite_integer | datatype code] for the initial data type
** of the result column.  ^the returned value is one of [sqlite_integer],
** [sqlite_float], [sqlite_text], [sqlite_blob], or [sqlite_null].  the value
** returned by sqlite3_column_type() is only meaningful if no type
** conversions have occurred as described below.  after a type conversion,
** the value returned by sqlite3_column_type() is undefined.  future
** versions of sqlite may change the behavior of sqlite3_column_type()
** following a type conversion.
**
** ^if the result is a blob or utf-8 string then the sqlite3_column_bytes()
** routine returns the number of bytes in that blob or string.
** ^if the result is a utf-16 string, then sqlite3_column_bytes() converts
** the string to utf-8 and then returns the number of bytes.
** ^if the result is a numeric value then sqlite3_column_bytes() uses
** [sqlite3_snprintf()] to convert that value to a utf-8 string and returns
** the number of bytes in that string.
** ^if the result is null, then sqlite3_column_bytes() returns zero.
**
** ^if the result is a blob or utf-16 string then the sqlite3_column_bytes16()
** routine returns the number of bytes in that blob or string.
** ^if the result is a utf-8 string, then sqlite3_column_bytes16() converts
** the string to utf-16 and then returns the number of bytes.
** ^if the result is a numeric value then sqlite3_column_bytes16() uses
** [sqlite3_snprintf()] to convert that value to a utf-16 string and returns
** the number of bytes in that string.
** ^if the result is null, then sqlite3_column_bytes16() returns zero.
**
** ^the values returned by [sqlite3_column_bytes()] and 
** [sqlite3_column_bytes16()] do not include the zero terminators at the end
** of the string.  ^for clarity: the values returned by
** [sqlite3_column_bytes()] and [sqlite3_column_bytes16()] are the number of
** bytes in the string, not the number of characters.
**
** ^strings returned by sqlite3_column_text() and sqlite3_column_text16(),
** even empty strings, are always zero-terminated.  ^the return
** value from sqlite3_column_blob() for a zero-length blob is a null pointer.
**
** ^the object returned by [sqlite3_column_value()] is an
** [unprotected sqlite3_value] object.  an unprotected sqlite3_value object
** may only be used with [sqlite3_bind_value()] and [sqlite3_result_value()].
** if the [unprotected sqlite3_value] object returned by
** [sqlite3_column_value()] is used in any other way, including calls
** to routines like [sqlite3_value_int()], [sqlite3_value_text()],
** or [sqlite3_value_bytes()], then the behavior is undefined.
**
** these routines attempt to convert the value where appropriate.  ^for
** example, if the internal representation is float and a text result
** is requested, [sqlite3_snprintf()] is used internally to perform the
** conversion automatically.  ^(the following table details the conversions
** that are applied:
**
** <blockquote>
** <table border="1">
** <tr><th> internal<br>type <th> requested<br>type <th>  conversion
**
** <tr><td>  null    <td> integer   <td> result is 0
** <tr><td>  null    <td>  float    <td> result is 0.0
** <tr><td>  null    <td>   text    <td> result is a null pointer
** <tr><td>  null    <td>   blob    <td> result is a null pointer
** <tr><td> integer  <td>  float    <td> convert from integer to float
** <tr><td> integer  <td>   text    <td> ascii rendering of the integer
** <tr><td> integer  <td>   blob    <td> same as integer->text
** <tr><td>  float   <td> integer   <td> [cast] to integer
** <tr><td>  float   <td>   text    <td> ascii rendering of the float
** <tr><td>  float   <td>   blob    <td> [cast] to blob
** <tr><td>  text    <td> integer   <td> [cast] to integer
** <tr><td>  text    <td>  float    <td> [cast] to real
** <tr><td>  text    <td>   blob    <td> no change
** <tr><td>  blob    <td> integer   <td> [cast] to integer
** <tr><td>  blob    <td>  float    <td> [cast] to real
** <tr><td>  blob    <td>   text    <td> add a zero terminator if needed
** </table>
** </blockquote>)^
**
** the table above makes reference to standard c library functions atoi()
** and atof().  sqlite does not really use these functions.  it has its
** own equivalent internal routines.  the atoi() and atof() names are
** used in the table for brevity and because they are familiar to most
** c programmers.
**
** note that when type conversions occur, pointers returned by prior
** calls to sqlite3_column_blob(), sqlite3_column_text(), and/or
** sqlite3_column_text16() may be invalidated.
** type conversions and pointer invalidations might occur
** in the following cases:
**
** <ul>
** <li> the initial content is a blob and sqlite3_column_text() or
**      sqlite3_column_text16() is called.  a zero-terminator might
**      need to be added to the string.</li>
** <li> the initial content is utf-8 text and sqlite3_column_bytes16() or
**      sqlite3_column_text16() is called.  the content must be converted
**      to utf-16.</li>
** <li> the initial content is utf-16 text and sqlite3_column_bytes() or
**      sqlite3_column_text() is called.  the content must be converted
**      to utf-8.</li>
** </ul>
**
** ^conversions between utf-16be and utf-16le are always done in place and do
** not invalidate a prior pointer, though of course the content of the buffer
** that the prior pointer references will have been modified.  other kinds
** of conversion are done in place when it is possible, but sometimes they
** are not possible and in those cases prior pointers are invalidated.
**
** the safest and easiest to remember policy is to invoke these routines
** in one of the following ways:
**
** <ul>
**  <li>sqlite3_column_text() followed by sqlite3_column_bytes()</li>
**  <li>sqlite3_column_blob() followed by sqlite3_column_bytes()</li>
**  <li>sqlite3_column_text16() followed by sqlite3_column_bytes16()</li>
** </ul>
**
** in other words, you should call sqlite3_column_text(),
** sqlite3_column_blob(), or sqlite3_column_text16() first to force the result
** into the desired format, then invoke sqlite3_column_bytes() or
** sqlite3_column_bytes16() to find the size of the result.  do not mix calls
** to sqlite3_column_text() or sqlite3_column_blob() with calls to
** sqlite3_column_bytes16(), and do not mix calls to sqlite3_column_text16()
** with calls to sqlite3_column_bytes().
**
** ^the pointers returned are valid until a type conversion occurs as
** described above, or until [sqlite3_step()] or [sqlite3_reset()] or
** [sqlite3_finalize()] is called.  ^the memory space used to hold strings
** and blobs is freed automatically.  do <b>not</b> pass the pointers returned
** from [sqlite3_column_blob()], [sqlite3_column_text()], etc. into
** [sqlite3_free()].
**
** ^(if a memory allocation error occurs during the evaluation of any
** of these routines, a default value is returned.  the default value
** is either the integer 0, the floating point number 0.0, or a null
** pointer.  subsequent calls to [sqlite3_errcode()] will return
** [sqlite_nomem].)^
*/
sqlite_api const void *sqlite3_column_blob(sqlite3_stmt*, int icol);
sqlite_api int sqlite3_column_bytes(sqlite3_stmt*, int icol);
sqlite_api int sqlite3_column_bytes16(sqlite3_stmt*, int icol);
sqlite_api double sqlite3_column_double(sqlite3_stmt*, int icol);
sqlite_api int sqlite3_column_int(sqlite3_stmt*, int icol);
sqlite_api sqlite3_int64 sqlite3_column_int64(sqlite3_stmt*, int icol);
sqlite_api const unsigned char *sqlite3_column_text(sqlite3_stmt*, int icol);
sqlite_api const void *sqlite3_column_text16(sqlite3_stmt*, int icol);
sqlite_api int sqlite3_column_type(sqlite3_stmt*, int icol);
sqlite_api sqlite3_value *sqlite3_column_value(sqlite3_stmt*, int icol);

/*
** capi3ref: destroy a prepared statement object
**
** ^the sqlite3_finalize() function is called to delete a [prepared statement].
** ^if the most recent evaluation of the statement encountered no errors
** or if the statement is never been evaluated, then sqlite3_finalize() returns
** sqlite_ok.  ^if the most recent evaluation of statement s failed, then
** sqlite3_finalize(s) returns the appropriate [error code] or
** [extended error code].
**
** ^the sqlite3_finalize(s) routine can be called at any point during
** the life cycle of [prepared statement] s:
** before statement s is ever evaluated, after
** one or more calls to [sqlite3_reset()], or after any call
** to [sqlite3_step()] regardless of whether or not the statement has
** completed execution.
**
** ^invoking sqlite3_finalize() on a null pointer is a harmless no-op.
**
** the application must finalize every [prepared statement] in order to avoid
** resource leaks.  it is a grievous error for the application to try to use
** a prepared statement after it has been finalized.  any use of a prepared
** statement after it has been finalized can result in undefined and
** undesirable behavior such as segfaults and heap corruption.
*/
sqlite_api int sqlite3_finalize(sqlite3_stmt *pstmt);

/*
** capi3ref: reset a prepared statement object
**
** the sqlite3_reset() function is called to reset a [prepared statement]
** object back to its initial state, ready to be re-executed.
** ^any sql statement variables that had values bound to them using
** the [sqlite3_bind_blob | sqlite3_bind_*() api] retain their values.
** use [sqlite3_clear_bindings()] to reset the bindings.
**
** ^the [sqlite3_reset(s)] interface resets the [prepared statement] s
** back to the beginning of its program.
**
** ^if the most recent call to [sqlite3_step(s)] for the
** [prepared statement] s returned [sqlite_row] or [sqlite_done],
** or if [sqlite3_step(s)] has never before been called on s,
** then [sqlite3_reset(s)] returns [sqlite_ok].
**
** ^if the most recent call to [sqlite3_step(s)] for the
** [prepared statement] s indicated an error, then
** [sqlite3_reset(s)] returns an appropriate [error code].
**
** ^the [sqlite3_reset(s)] interface does not change the values
** of any [sqlite3_bind_blob|bindings] on the [prepared statement] s.
*/
sqlite_api int sqlite3_reset(sqlite3_stmt *pstmt);

/*
** capi3ref: create or redefine sql functions
** keywords: {function creation routines}
** keywords: {application-defined sql function}
** keywords: {application-defined sql functions}
**
** ^these functions (collectively known as "function creation routines")
** are used to add sql functions or aggregates or to redefine the behavior
** of existing sql functions or aggregates.  the only differences between
** these routines are the text encoding expected for
** the second parameter (the name of the function being created)
** and the presence or absence of a destructor callback for
** the application data pointer.
**
** ^the first parameter is the [database connection] to which the sql
** function is to be added.  ^if an application uses more than one database
** connection then application-defined sql functions must be added
** to each database connection separately.
**
** ^the second parameter is the name of the sql function to be created or
** redefined.  ^the length of the name is limited to 255 bytes in a utf-8
** representation, exclusive of the zero-terminator.  ^note that the name
** length limit is in utf-8 bytes, not characters nor utf-16 bytes.  
** ^any attempt to create a function with a longer name
** will result in [sqlite_misuse] being returned.
**
** ^the third parameter (narg)
** is the number of arguments that the sql function or
** aggregate takes. ^if this parameter is -1, then the sql function or
** aggregate may take any number of arguments between 0 and the limit
** set by [sqlite3_limit]([sqlite_limit_function_arg]).  if the third
** parameter is less than -1 or greater than 127 then the behavior is
** undefined.
**
** ^the fourth parameter, etextrep, specifies what
** [sqlite_utf8 | text encoding] this sql function prefers for
** its parameters.  the application should set this parameter to
** [sqlite_utf16le] if the function implementation invokes 
** [sqlite3_value_text16le()] on an input, or [sqlite_utf16be] if the
** implementation invokes [sqlite3_value_text16be()] on an input, or
** [sqlite_utf16] if [sqlite3_value_text16()] is used, or [sqlite_utf8]
** otherwise.  ^the same sql function may be registered multiple times using
** different preferred text encodings, with different implementations for
** each encoding.
** ^when multiple implementations of the same function are available, sqlite
** will pick the one that involves the least amount of data conversion.
**
** ^the fourth parameter may optionally be ored with [sqlite_deterministic]
** to signal that the function will always return the same result given
** the same inputs within a single sql statement.  most sql functions are
** deterministic.  the built-in [random()] sql function is an example of a
** function that is not deterministic.  the sqlite query planner is able to
** perform additional optimizations on deterministic functions, so use
** of the [sqlite_deterministic] flag is recommended where possible.
**
** ^(the fifth parameter is an arbitrary pointer.  the implementation of the
** function can gain access to this pointer using [sqlite3_user_data()].)^
**
** ^the sixth, seventh and eighth parameters, xfunc, xstep and xfinal, are
** pointers to c-language functions that implement the sql function or
** aggregate. ^a scalar sql function requires an implementation of the xfunc
** callback only; null pointers must be passed as the xstep and xfinal
** parameters. ^an aggregate sql function requires an implementation of xstep
** and xfinal and null pointer must be passed for xfunc. ^to delete an existing
** sql function or aggregate, pass null pointers for all three function
** callbacks.
**
** ^(if the ninth parameter to sqlite3_create_function_v2() is not null,
** then it is destructor for the application data pointer. 
** the destructor is invoked when the function is deleted, either by being
** overloaded or when the database connection closes.)^
** ^the destructor is also invoked if the call to
** sqlite3_create_function_v2() fails.
** ^when the destructor callback of the tenth parameter is invoked, it
** is passed a single argument which is a copy of the application data 
** pointer which was the fifth parameter to sqlite3_create_function_v2().
**
** ^it is permitted to register multiple implementations of the same
** functions with the same name but with either differing numbers of
** arguments or differing preferred text encodings.  ^sqlite will use
** the implementation that most closely matches the way in which the
** sql function is used.  ^a function implementation with a non-negative
** narg parameter is a better match than a function implementation with
** a negative narg.  ^a function where the preferred text encoding
** matches the database encoding is a better
** match than a function where the encoding is different.  
** ^a function where the encoding difference is between utf16le and utf16be
** is a closer match than a function where the encoding difference is
** between utf8 and utf16.
**
** ^built-in functions may be overloaded by new application-defined functions.
**
** ^an application-defined function is permitted to call other
** sqlite interfaces.  however, such calls must not
** close the database connection nor finalize or reset the prepared
** statement in which the function is running.
*/
sqlite_api int sqlite3_create_function(
  sqlite3 *db,
  const char *zfunctionname,
  int narg,
  int etextrep,
  void *papp,
  void (*xfunc)(sqlite3_context*,int,sqlite3_value**),
  void (*xstep)(sqlite3_context*,int,sqlite3_value**),
  void (*xfinal)(sqlite3_context*)
);
sqlite_api int sqlite3_create_function16(
  sqlite3 *db,
  const void *zfunctionname,
  int narg,
  int etextrep,
  void *papp,
  void (*xfunc)(sqlite3_context*,int,sqlite3_value**),
  void (*xstep)(sqlite3_context*,int,sqlite3_value**),
  void (*xfinal)(sqlite3_context*)
);
sqlite_api int sqlite3_create_function_v2(
  sqlite3 *db,
  const char *zfunctionname,
  int narg,
  int etextrep,
  void *papp,
  void (*xfunc)(sqlite3_context*,int,sqlite3_value**),
  void (*xstep)(sqlite3_context*,int,sqlite3_value**),
  void (*xfinal)(sqlite3_context*),
  void(*xdestroy)(void*)
);

/*
** capi3ref: text encodings
**
** these constant define integer codes that represent the various
** text encodings supported by sqlite.
*/
#define sqlite_utf8           1
#define sqlite_utf16le        2
#define sqlite_utf16be        3
#define sqlite_utf16          4    /* use native byte order */
#define sqlite_any            5    /* deprecated */
#define sqlite_utf16_aligned  8    /* sqlite3_create_collation only */

/*
** capi3ref: function flags
**
** these constants may be ored together with the 
** [sqlite_utf8 | preferred text encoding] as the fourth argument
** to [sqlite3_create_function()], [sqlite3_create_function16()], or
** [sqlite3_create_function_v2()].
*/
#define sqlite_deterministic    0x800

/*
** capi3ref: deprecated functions
** deprecated
**
** these functions are [deprecated].  in order to maintain
** backwards compatibility with older code, these functions continue 
** to be supported.  however, new applications should avoid
** the use of these functions.  to help encourage people to avoid
** using these functions, we are not going to tell you what they do.
*/
#ifndef sqlite_omit_deprecated
sqlite_api sqlite_deprecated int sqlite3_aggregate_count(sqlite3_context*);
sqlite_api sqlite_deprecated int sqlite3_expired(sqlite3_stmt*);
sqlite_api sqlite_deprecated int sqlite3_transfer_bindings(sqlite3_stmt*, sqlite3_stmt*);
sqlite_api sqlite_deprecated int sqlite3_global_recover(void);
sqlite_api sqlite_deprecated void sqlite3_thread_cleanup(void);
sqlite_api sqlite_deprecated int sqlite3_memory_alarm(void(*)(void*,sqlite3_int64,int),
                      void*,sqlite3_int64);
#endif

/*
** capi3ref: obtaining sql function parameter values
**
** the c-language implementation of sql functions and aggregates uses
** this set of interface routines to access the parameter values on
** the function or aggregate.
**
** the xfunc (for scalar functions) or xstep (for aggregates) parameters
** to [sqlite3_create_function()] and [sqlite3_create_function16()]
** define callbacks that implement the sql functions and aggregates.
** the 3rd parameter to these callbacks is an array of pointers to
** [protected sqlite3_value] objects.  there is one [sqlite3_value] object for
** each parameter to the sql function.  these routines are used to
** extract values from the [sqlite3_value] objects.
**
** these routines work only with [protected sqlite3_value] objects.
** any attempt to use these routines on an [unprotected sqlite3_value]
** object results in undefined behavior.
**
** ^these routines work just like the corresponding [column access functions]
** except that these routines take a single [protected sqlite3_value] object
** pointer instead of a [sqlite3_stmt*] pointer and an integer column number.
**
** ^the sqlite3_value_text16() interface extracts a utf-16 string
** in the native byte-order of the host machine.  ^the
** sqlite3_value_text16be() and sqlite3_value_text16le() interfaces
** extract utf-16 strings as big-endian and little-endian respectively.
**
** ^(the sqlite3_value_numeric_type() interface attempts to apply
** numeric affinity to the value.  this means that an attempt is
** made to convert the value to an integer or floating point.  if
** such a conversion is possible without loss of information (in other
** words, if the value is a string that looks like a number)
** then the conversion is performed.  otherwise no conversion occurs.
** the [sqlite_integer | datatype] after conversion is returned.)^
**
** please pay particular attention to the fact that the pointer returned
** from [sqlite3_value_blob()], [sqlite3_value_text()], or
** [sqlite3_value_text16()] can be invalidated by a subsequent call to
** [sqlite3_value_bytes()], [sqlite3_value_bytes16()], [sqlite3_value_text()],
** or [sqlite3_value_text16()].
**
** these routines must be called from the same thread as
** the sql function that supplied the [sqlite3_value*] parameters.
*/
sqlite_api const void *sqlite3_value_blob(sqlite3_value*);
sqlite_api int sqlite3_value_bytes(sqlite3_value*);
sqlite_api int sqlite3_value_bytes16(sqlite3_value*);
sqlite_api double sqlite3_value_double(sqlite3_value*);
sqlite_api int sqlite3_value_int(sqlite3_value*);
sqlite_api sqlite3_int64 sqlite3_value_int64(sqlite3_value*);
sqlite_api const unsigned char *sqlite3_value_text(sqlite3_value*);
sqlite_api const void *sqlite3_value_text16(sqlite3_value*);
sqlite_api const void *sqlite3_value_text16le(sqlite3_value*);
sqlite_api const void *sqlite3_value_text16be(sqlite3_value*);
sqlite_api int sqlite3_value_type(sqlite3_value*);
sqlite_api int sqlite3_value_numeric_type(sqlite3_value*);

/*
** capi3ref: obtain aggregate function context
**
** implementations of aggregate sql functions use this
** routine to allocate memory for storing their state.
**
** ^the first time the sqlite3_aggregate_context(c,n) routine is called 
** for a particular aggregate function, sqlite
** allocates n of memory, zeroes out that memory, and returns a pointer
** to the new memory. ^on second and subsequent calls to
** sqlite3_aggregate_context() for the same aggregate function instance,
** the same buffer is returned.  sqlite3_aggregate_context() is normally
** called once for each invocation of the xstep callback and then one
** last time when the xfinal callback is invoked.  ^(when no rows match
** an aggregate query, the xstep() callback of the aggregate function
** implementation is never called and xfinal() is called exactly once.
** in those cases, sqlite3_aggregate_context() might be called for the
** first time from within xfinal().)^
**
** ^the sqlite3_aggregate_context(c,n) routine returns a null pointer 
** when first called if n is less than or equal to zero or if a memory
** allocate error occurs.
**
** ^(the amount of space allocated by sqlite3_aggregate_context(c,n) is
** determined by the n parameter on first successful call.  changing the
** value of n in subsequent call to sqlite3_aggregate_context() within
** the same aggregate function instance will not resize the memory
** allocation.)^  within the xfinal callback, it is customary to set
** n=0 in calls to sqlite3_aggregate_context(c,n) so that no 
** pointless memory allocations occur.
**
** ^sqlite automatically frees the memory allocated by 
** sqlite3_aggregate_context() when the aggregate query concludes.
**
** the first parameter must be a copy of the
** [sqlite3_context | sql function context] that is the first parameter
** to the xstep or xfinal callback routine that implements the aggregate
** function.
**
** this routine must be called from the same thread in which
** the aggregate sql function is running.
*/
sqlite_api void *sqlite3_aggregate_context(sqlite3_context*, int nbytes);

/*
** capi3ref: user data for functions
**
** ^the sqlite3_user_data() interface returns a copy of
** the pointer that was the puserdata parameter (the 5th parameter)
** of the [sqlite3_create_function()]
** and [sqlite3_create_function16()] routines that originally
** registered the application defined function.
**
** this routine must be called from the same thread in which
** the application-defined function is running.
*/
sqlite_api void *sqlite3_user_data(sqlite3_context*);

/*
** capi3ref: database connection for functions
**
** ^the sqlite3_context_db_handle() interface returns a copy of
** the pointer to the [database connection] (the 1st parameter)
** of the [sqlite3_create_function()]
** and [sqlite3_create_function16()] routines that originally
** registered the application defined function.
*/
sqlite_api sqlite3 *sqlite3_context_db_handle(sqlite3_context*);

/*
** capi3ref: function auxiliary data
**
** these functions may be used by (non-aggregate) sql functions to
** associate metadata with argument values. if the same value is passed to
** multiple invocations of the same sql function during query execution, under
** some circumstances the associated metadata may be preserved.  an example
** of where this might be useful is in a regular-expression matching
** function. the compiled version of the regular expression can be stored as
** metadata associated with the pattern string.  
** then as long as the pattern string remains the same,
** the compiled regular expression can be reused on multiple
** invocations of the same function.
**
** ^the sqlite3_get_auxdata() interface returns a pointer to the metadata
** associated by the sqlite3_set_auxdata() function with the nth argument
** value to the application-defined function. ^if there is no metadata
** associated with the function argument, this sqlite3_get_auxdata() interface
** returns a null pointer.
**
** ^the sqlite3_set_auxdata(c,n,p,x) interface saves p as metadata for the n-th
** argument of the application-defined function.  ^subsequent
** calls to sqlite3_get_auxdata(c,n) return p from the most recent
** sqlite3_set_auxdata(c,n,p,x) call if the metadata is still valid or
** null if the metadata has been discarded.
** ^after each call to sqlite3_set_auxdata(c,n,p,x) where x is not null,
** sqlite will invoke the destructor function x with parameter p exactly
** once, when the metadata is discarded.
** sqlite is free to discard the metadata at any time, including: <ul>
** <li> when the corresponding function parameter changes, or
** <li> when [sqlite3_reset()] or [sqlite3_finalize()] is called for the
**      sql statement, or
** <li> when sqlite3_set_auxdata() is invoked again on the same parameter, or
** <li> during the original sqlite3_set_auxdata() call when a memory 
**      allocation error occurs. </ul>)^
**
** note the last bullet in particular.  the destructor x in 
** sqlite3_set_auxdata(c,n,p,x) might be called immediately, before the
** sqlite3_set_auxdata() interface even returns.  hence sqlite3_set_auxdata()
** should be called near the end of the function implementation and the
** function implementation should not make any use of p after
** sqlite3_set_auxdata() has been called.
**
** ^(in practice, metadata is preserved between function calls for
** function parameters that are compile-time constants, including literal
** values and [parameters] and expressions composed from the same.)^
**
** these routines must be called from the same thread in which
** the sql function is running.
*/
sqlite_api void *sqlite3_get_auxdata(sqlite3_context*, int n);
sqlite_api void sqlite3_set_auxdata(sqlite3_context*, int n, void*, void (*)(void*));


/*
** capi3ref: constants defining special destructor behavior
**
** these are special values for the destructor that is passed in as the
** final argument to routines like [sqlite3_result_blob()].  ^if the destructor
** argument is sqlite_static, it means that the content pointer is constant
** and will never change.  it does not need to be destroyed.  ^the
** sqlite_transient value means that the content will likely change in
** the near future and that sqlite should make its own private copy of
** the content before returning.
**
** the typedef is necessary to work around problems in certain
** c++ compilers.
*/
typedef void (*sqlite3_destructor_type)(void*);
#define sqlite_static      ((sqlite3_destructor_type)0)
#define sqlite_transient   ((sqlite3_destructor_type)-1)

/*
** capi3ref: setting the result of an sql function
**
** these routines are used by the xfunc or xfinal callbacks that
** implement sql functions and aggregates.  see
** [sqlite3_create_function()] and [sqlite3_create_function16()]
** for additional information.
**
** these functions work very much like the [parameter binding] family of
** functions used to bind values to host parameters in prepared statements.
** refer to the [sql parameter] documentation for additional information.
**
** ^the sqlite3_result_blob() interface sets the result from
** an application-defined function to be the blob whose content is pointed
** to by the second parameter and which is n bytes long where n is the
** third parameter.
**
** ^the sqlite3_result_zeroblob() interfaces set the result of
** the application-defined function to be a blob containing all zero
** bytes and n bytes in size, where n is the value of the 2nd parameter.
**
** ^the sqlite3_result_double() interface sets the result from
** an application-defined function to be a floating point value specified
** by its 2nd argument.
**
** ^the sqlite3_result_error() and sqlite3_result_error16() functions
** cause the implemented sql function to throw an exception.
** ^sqlite uses the string pointed to by the
** 2nd parameter of sqlite3_result_error() or sqlite3_result_error16()
** as the text of an error message.  ^sqlite interprets the error
** message string from sqlite3_result_error() as utf-8. ^sqlite
** interprets the string from sqlite3_result_error16() as utf-16 in native
** byte order.  ^if the third parameter to sqlite3_result_error()
** or sqlite3_result_error16() is negative then sqlite takes as the error
** message all text up through the first zero character.
** ^if the third parameter to sqlite3_result_error() or
** sqlite3_result_error16() is non-negative then sqlite takes that many
** bytes (not characters) from the 2nd parameter as the error message.
** ^the sqlite3_result_error() and sqlite3_result_error16()
** routines make a private copy of the error message text before
** they return.  hence, the calling function can deallocate or
** modify the text after they return without harm.
** ^the sqlite3_result_error_code() function changes the error code
** returned by sqlite as a result of an error in a function.  ^by default,
** the error code is sqlite_error.  ^a subsequent call to sqlite3_result_error()
** or sqlite3_result_error16() resets the error code to sqlite_error.
**
** ^the sqlite3_result_error_toobig() interface causes sqlite to throw an
** error indicating that a string or blob is too long to represent.
**
** ^the sqlite3_result_error_nomem() interface causes sqlite to throw an
** error indicating that a memory allocation failed.
**
** ^the sqlite3_result_int() interface sets the return value
** of the application-defined function to be the 32-bit signed integer
** value given in the 2nd argument.
** ^the sqlite3_result_int64() interface sets the return value
** of the application-defined function to be the 64-bit signed integer
** value given in the 2nd argument.
**
** ^the sqlite3_result_null() interface sets the return value
** of the application-defined function to be null.
**
** ^the sqlite3_result_text(), sqlite3_result_text16(),
** sqlite3_result_text16le(), and sqlite3_result_text16be() interfaces
** set the return value of the application-defined function to be
** a text string which is represented as utf-8, utf-16 native byte order,
** utf-16 little endian, or utf-16 big endian, respectively.
** ^the sqlite3_result_text64() interface sets the return value of an
** application-defined function to be a text string in an encoding
** specified by the fifth (and last) parameter, which must be one
** of [sqlite_utf8], [sqlite_utf16], [sqlite_utf16be], or [sqlite_utf16le].
** ^sqlite takes the text result from the application from
** the 2nd parameter of the sqlite3_result_text* interfaces.
** ^if the 3rd parameter to the sqlite3_result_text* interfaces
** is negative, then sqlite takes result text from the 2nd parameter
** through the first zero character.
** ^if the 3rd parameter to the sqlite3_result_text* interfaces
** is non-negative, then as many bytes (not characters) of the text
** pointed to by the 2nd parameter are taken as the application-defined
** function result.  if the 3rd parameter is non-negative, then it
** must be the byte offset into the string where the nul terminator would
** appear if the string where nul terminated.  if any nul characters occur
** in the string at a byte offset that is less than the value of the 3rd
** parameter, then the resulting string will contain embedded nuls and the
** result of expressions operating on strings with embedded nuls is undefined.
** ^if the 4th parameter to the sqlite3_result_text* interfaces
** or sqlite3_result_blob is a non-null pointer, then sqlite calls that
** function as the destructor on the text or blob result when it has
** finished using that result.
** ^if the 4th parameter to the sqlite3_result_text* interfaces or to
** sqlite3_result_blob is the special constant sqlite_static, then sqlite
** assumes that the text or blob result is in constant space and does not
** copy the content of the parameter nor call a destructor on the content
** when it has finished using that result.
** ^if the 4th parameter to the sqlite3_result_text* interfaces
** or sqlite3_result_blob is the special constant sqlite_transient
** then sqlite makes a copy of the result into space obtained from
** from [sqlite3_malloc()] before it returns.
**
** ^the sqlite3_result_value() interface sets the result of
** the application-defined function to be a copy the
** [unprotected sqlite3_value] object specified by the 2nd parameter.  ^the
** sqlite3_result_value() interface makes a copy of the [sqlite3_value]
** so that the [sqlite3_value] specified in the parameter may change or
** be deallocated after sqlite3_result_value() returns without harm.
** ^a [protected sqlite3_value] object may always be used where an
** [unprotected sqlite3_value] object is required, so either
** kind of [sqlite3_value] object can be used with this interface.
**
** if these routines are called from within the different thread
** than the one containing the application-defined function that received
** the [sqlite3_context] pointer, the results are undefined.
*/
sqlite_api void sqlite3_result_blob(sqlite3_context*, const void*, int, void(*)(void*));
sqlite_api void sqlite3_result_blob64(sqlite3_context*,const void*,sqlite3_uint64,void(*)(void*));
sqlite_api void sqlite3_result_double(sqlite3_context*, double);
sqlite_api void sqlite3_result_error(sqlite3_context*, const char*, int);
sqlite_api void sqlite3_result_error16(sqlite3_context*, const void*, int);
sqlite_api void sqlite3_result_error_toobig(sqlite3_context*);
sqlite_api void sqlite3_result_error_nomem(sqlite3_context*);
sqlite_api void sqlite3_result_error_code(sqlite3_context*, int);
sqlite_api void sqlite3_result_int(sqlite3_context*, int);
sqlite_api void sqlite3_result_int64(sqlite3_context*, sqlite3_int64);
sqlite_api void sqlite3_result_null(sqlite3_context*);
sqlite_api void sqlite3_result_text(sqlite3_context*, const char*, int, void(*)(void*));
sqlite_api void sqlite3_result_text64(sqlite3_context*, const char*,sqlite3_uint64,
                           void(*)(void*), unsigned char encoding);
sqlite_api void sqlite3_result_text16(sqlite3_context*, const void*, int, void(*)(void*));
sqlite_api void sqlite3_result_text16le(sqlite3_context*, const void*, int,void(*)(void*));
sqlite_api void sqlite3_result_text16be(sqlite3_context*, const void*, int,void(*)(void*));
sqlite_api void sqlite3_result_value(sqlite3_context*, sqlite3_value*);
sqlite_api void sqlite3_result_zeroblob(sqlite3_context*, int n);

/*
** capi3ref: define new collating sequences
**
** ^these functions add, remove, or modify a [collation] associated
** with the [database connection] specified as the first argument.
**
** ^the name of the collation is a utf-8 string
** for sqlite3_create_collation() and sqlite3_create_collation_v2()
** and a utf-16 string in native byte order for sqlite3_create_collation16().
** ^collation names that compare equal according to [sqlite3_strnicmp()] are
** considered to be the same name.
**
** ^(the third argument (etextrep) must be one of the constants:
** <ul>
** <li> [sqlite_utf8],
** <li> [sqlite_utf16le],
** <li> [sqlite_utf16be],
** <li> [sqlite_utf16], or
** <li> [sqlite_utf16_aligned].
** </ul>)^
** ^the etextrep argument determines the encoding of strings passed
** to the collating function callback, xcallback.
** ^the [sqlite_utf16] and [sqlite_utf16_aligned] values for etextrep
** force strings to be utf16 with native byte order.
** ^the [sqlite_utf16_aligned] value for etextrep forces strings to begin
** on an even byte address.
**
** ^the fourth argument, parg, is an application data pointer that is passed
** through as the first argument to the collating function callback.
**
** ^the fifth argument, xcallback, is a pointer to the collating function.
** ^multiple collating functions can be registered using the same name but
** with different etextrep parameters and sqlite will use whichever
** function requires the least amount of data transformation.
** ^if the xcallback argument is null then the collating function is
** deleted.  ^when all collating functions having the same name are deleted,
** that collation is no longer usable.
**
** ^the collating function callback is invoked with a copy of the parg 
** application data pointer and with two strings in the encoding specified
** by the etextrep argument.  the collating function must return an
** integer that is negative, zero, or positive
** if the first string is less than, equal to, or greater than the second,
** respectively.  a collating function must always return the same answer
** given the same inputs.  if two or more collating functions are registered
** to the same collation name (using different etextrep values) then all
** must give an equivalent answer when invoked with equivalent strings.
** the collating function must obey the following properties for all
** strings a, b, and c:
**
** <ol>
** <li> if a==b then b==a.
** <li> if a==b and b==c then a==c.
** <li> if a&lt;b then b&gt;a.
** <li> if a&lt;b and b&lt;c then a&lt;c.
** </ol>
**
** if a collating function fails any of the above constraints and that
** collating function is  registered and used, then the behavior of sqlite
** is undefined.
**
** ^the sqlite3_create_collation_v2() works like sqlite3_create_collation()
** with the addition that the xdestroy callback is invoked on parg when
** the collating function is deleted.
** ^collating functions are deleted when they are overridden by later
** calls to the collation creation functions or when the
** [database connection] is closed using [sqlite3_close()].
**
** ^the xdestroy callback is <u>not</u> called if the 
** sqlite3_create_collation_v2() function fails.  applications that invoke
** sqlite3_create_collation_v2() with a non-null xdestroy argument should 
** check the return code and dispose of the application data pointer
** themselves rather than expecting sqlite to deal with it for them.
** this is different from every other sqlite interface.  the inconsistency 
** is unfortunate but cannot be changed without breaking backwards 
** compatibility.
**
** see also:  [sqlite3_collation_needed()] and [sqlite3_collation_needed16()].
*/
sqlite_api int sqlite3_create_collation(
  sqlite3*, 
  const char *zname, 
  int etextrep, 
  void *parg,
  int(*xcompare)(void*,int,const void*,int,const void*)
);
sqlite_api int sqlite3_create_collation_v2(
  sqlite3*, 
  const char *zname, 
  int etextrep, 
  void *parg,
  int(*xcompare)(void*,int,const void*,int,const void*),
  void(*xdestroy)(void*)
);
sqlite_api int sqlite3_create_collation16(
  sqlite3*, 
  const void *zname,
  int etextrep, 
  void *parg,
  int(*xcompare)(void*,int,const void*,int,const void*)
);

/*
** capi3ref: collation needed callbacks
**
** ^to avoid having to register all collation sequences before a database
** can be used, a single callback function may be registered with the
** [database connection] to be invoked whenever an undefined collation
** sequence is required.
**
** ^if the function is registered using the sqlite3_collation_needed() api,
** then it is passed the names of undefined collation sequences as strings
** encoded in utf-8. ^if sqlite3_collation_needed16() is used,
** the names are passed as utf-16 in machine native byte order.
** ^a call to either function replaces the existing collation-needed callback.
**
** ^(when the callback is invoked, the first argument passed is a copy
** of the second argument to sqlite3_collation_needed() or
** sqlite3_collation_needed16().  the second argument is the database
** connection.  the third argument is one of [sqlite_utf8], [sqlite_utf16be],
** or [sqlite_utf16le], indicating the most desirable form of the collation
** sequence function required.  the fourth parameter is the name of the
** required collation sequence.)^
**
** the callback function should register the desired collation using
** [sqlite3_create_collation()], [sqlite3_create_collation16()], or
** [sqlite3_create_collation_v2()].
*/
sqlite_api int sqlite3_collation_needed(
  sqlite3*, 
  void*, 
  void(*)(void*,sqlite3*,int etextrep,const char*)
);
sqlite_api int sqlite3_collation_needed16(
  sqlite3*, 
  void*,
  void(*)(void*,sqlite3*,int etextrep,const void*)
);

#ifdef sqlite_has_codec
/*
** specify the key for an encrypted database.  this routine should be
** called right after sqlite3_open().
**
** the code to implement this api is not available in the public release
** of sqlite.
*/
sqlite_api int sqlite3_key(
  sqlite3 *db,                   /* database to be rekeyed */
  const void *pkey, int nkey     /* the key */
);
sqlite_api int sqlite3_key_v2(
  sqlite3 *db,                   /* database to be rekeyed */
  const char *zdbname,           /* name of the database */
  const void *pkey, int nkey     /* the key */
);

/*
** change the key on an open database.  if the current database is not
** encrypted, this routine will encrypt it.  if pnew==0 or nnew==0, the
** database is decrypted.
**
** the code to implement this api is not available in the public release
** of sqlite.
*/
sqlite_api int sqlite3_rekey(
  sqlite3 *db,                   /* database to be rekeyed */
  const void *pkey, int nkey     /* the new key */
);
sqlite_api int sqlite3_rekey_v2(
  sqlite3 *db,                   /* database to be rekeyed */
  const char *zdbname,           /* name of the database */
  const void *pkey, int nkey     /* the new key */
);

/*
** specify the activation key for a see database.  unless 
** activated, none of the see routines will work.
*/
sqlite_api void sqlite3_activate_see(
  const char *zpassphrase        /* activation phrase */
);
#endif

#ifdef sqlite_enable_cerod
/*
** specify the activation key for a cerod database.  unless 
** activated, none of the cerod routines will work.
*/
sqlite_api void sqlite3_activate_cerod(
  const char *zpassphrase        /* activation phrase */
);
#endif

/*
** capi3ref: suspend execution for a short time
**
** the sqlite3_sleep() function causes the current thread to suspend execution
** for at least a number of milliseconds specified in its parameter.
**
** if the operating system does not support sleep requests with
** millisecond time resolution, then the time will be rounded up to
** the nearest second. the number of milliseconds of sleep actually
** requested from the operating system is returned.
**
** ^sqlite implements this interface by calling the xsleep()
** method of the default [sqlite3_vfs] object.  if the xsleep() method
** of the default vfs is not implemented correctly, or not implemented at
** all, then the behavior of sqlite3_sleep() may deviate from the description
** in the previous paragraphs.
*/
sqlite_api int sqlite3_sleep(int);

/*
** capi3ref: name of the folder holding temporary files
**
** ^(if this global variable is made to point to a string which is
** the name of a folder (a.k.a. directory), then all temporary files
** created by sqlite when using a built-in [sqlite3_vfs | vfs]
** will be placed in that directory.)^  ^if this variable
** is a null pointer, then sqlite performs a search for an appropriate
** temporary file directory.
**
** applications are strongly discouraged from using this global variable.
** it is required to set a temporary folder on windows runtime (winrt).
** but for all other platforms, it is highly recommended that applications
** neither read nor write this variable.  this global variable is a relic
** that exists for backwards compatibility of legacy applications and should
** be avoided in new projects.
**
** it is not safe to read or modify this variable in more than one
** thread at a time.  it is not safe to read or modify this variable
** if a [database connection] is being used at the same time in a separate
** thread.
** it is intended that this variable be set once
** as part of process initialization and before any sqlite interface
** routines have been called and that this variable remain unchanged
** thereafter.
**
** ^the [temp_store_directory pragma] may modify this variable and cause
** it to point to memory obtained from [sqlite3_malloc].  ^furthermore,
** the [temp_store_directory pragma] always assumes that any string
** that this variable points to is held in memory obtained from 
** [sqlite3_malloc] and the pragma may attempt to free that memory
** using [sqlite3_free].
** hence, if this variable is modified directly, either it should be
** made null or made to point to memory obtained from [sqlite3_malloc]
** or else the use of the [temp_store_directory pragma] should be avoided.
** except when requested by the [temp_store_directory pragma], sqlite
** does not free the memory that sqlite3_temp_directory points to.  if
** the application wants that memory to be freed, it must do
** so itself, taking care to only do so after all [database connection]
** objects have been destroyed.
**
** <b>note to windows runtime users:</b>  the temporary directory must be set
** prior to calling [sqlite3_open] or [sqlite3_open_v2].  otherwise, various
** features that require the use of temporary files may fail.  here is an
** example of how to do this using c++ with the windows runtime:
**
** <blockquote><pre>
** lpcwstr zpath = windows::storage::applicationdata::current->
** &nbsp;     temporaryfolder->path->data();
** char zpathbuf&#91;max_path + 1&#93;;
** memset(zpathbuf, 0, sizeof(zpathbuf));
** widechartomultibyte(cp_utf8, 0, zpath, -1, zpathbuf, sizeof(zpathbuf),
** &nbsp;     null, null);
** sqlite3_temp_directory = sqlite3_mprintf("%s", zpathbuf);
** </pre></blockquote>
*/
sqlite_api sqlite_extern char *sqlite3_temp_directory;

/*
** capi3ref: name of the folder holding database files
**
** ^(if this global variable is made to point to a string which is
** the name of a folder (a.k.a. directory), then all database files
** specified with a relative pathname and created or accessed by
** sqlite when using a built-in windows [sqlite3_vfs | vfs] will be assumed
** to be relative to that directory.)^ ^if this variable is a null
** pointer, then sqlite assumes that all database files specified
** with a relative pathname are relative to the current directory
** for the process.  only the windows vfs makes use of this global
** variable; it is ignored by the unix vfs.
**
** changing the value of this variable while a database connection is
** open can result in a corrupt database.
**
** it is not safe to read or modify this variable in more than one
** thread at a time.  it is not safe to read or modify this variable
** if a [database connection] is being used at the same time in a separate
** thread.
** it is intended that this variable be set once
** as part of process initialization and before any sqlite interface
** routines have been called and that this variable remain unchanged
** thereafter.
**
** ^the [data_store_directory pragma] may modify this variable and cause
** it to point to memory obtained from [sqlite3_malloc].  ^furthermore,
** the [data_store_directory pragma] always assumes that any string
** that this variable points to is held in memory obtained from 
** [sqlite3_malloc] and the pragma may attempt to free that memory
** using [sqlite3_free].
** hence, if this variable is modified directly, either it should be
** made null or made to point to memory obtained from [sqlite3_malloc]
** or else the use of the [data_store_directory pragma] should be avoided.
*/
sqlite_api sqlite_extern char *sqlite3_data_directory;

/*
** capi3ref: test for auto-commit mode
** keywords: {autocommit mode}
**
** ^the sqlite3_get_autocommit() interface returns non-zero or
** zero if the given database connection is or is not in autocommit mode,
** respectively.  ^autocommit mode is on by default.
** ^autocommit mode is disabled by a [begin] statement.
** ^autocommit mode is re-enabled by a [commit] or [rollback].
**
** if certain kinds of errors occur on a statement within a multi-statement
** transaction (errors including [sqlite_full], [sqlite_ioerr],
** [sqlite_nomem], [sqlite_busy], and [sqlite_interrupt]) then the
** transaction might be rolled back automatically.  the only way to
** find out whether sqlite automatically rolled back the transaction after
** an error is to use this function.
**
** if another thread changes the autocommit status of the database
** connection while this routine is running, then the return value
** is undefined.
*/
sqlite_api int sqlite3_get_autocommit(sqlite3*);

/*
** capi3ref: find the database handle of a prepared statement
**
** ^the sqlite3_db_handle interface returns the [database connection] handle
** to which a [prepared statement] belongs.  ^the [database connection]
** returned by sqlite3_db_handle is the same [database connection]
** that was the first argument
** to the [sqlite3_prepare_v2()] call (or its variants) that was used to
** create the statement in the first place.
*/
sqlite_api sqlite3 *sqlite3_db_handle(sqlite3_stmt*);

/*
** capi3ref: return the filename for a database connection
**
** ^the sqlite3_db_filename(d,n) interface returns a pointer to a filename
** associated with database n of connection d.  ^the main database file
** has the name "main".  if there is no attached database n on the database
** connection d, or if database n is a temporary or in-memory database, then
** a null pointer is returned.
**
** ^the filename returned by this function is the output of the
** xfullpathname method of the [vfs].  ^in other words, the filename
** will be an absolute pathname, even if the filename used
** to open the database originally was a uri or relative pathname.
*/
sqlite_api const char *sqlite3_db_filename(sqlite3 *db, const char *zdbname);

/*
** capi3ref: determine if a database is read-only
**
** ^the sqlite3_db_readonly(d,n) interface returns 1 if the database n
** of connection d is read-only, 0 if it is read/write, or -1 if n is not
** the name of a database on connection d.
*/
sqlite_api int sqlite3_db_readonly(sqlite3 *db, const char *zdbname);

/*
** capi3ref: find the next prepared statement
**
** ^this interface returns a pointer to the next [prepared statement] after
** pstmt associated with the [database connection] pdb.  ^if pstmt is null
** then this interface returns a pointer to the first prepared statement
** associated with the database connection pdb.  ^if no prepared statement
** satisfies the conditions of this routine, it returns null.
**
** the [database connection] pointer d in a call to
** [sqlite3_next_stmt(d,s)] must refer to an open database
** connection and in particular must not be a null pointer.
*/
sqlite_api sqlite3_stmt *sqlite3_next_stmt(sqlite3 *pdb, sqlite3_stmt *pstmt);

/*
** capi3ref: commit and rollback notification callbacks
**
** ^the sqlite3_commit_hook() interface registers a callback
** function to be invoked whenever a transaction is [commit | committed].
** ^any callback set by a previous call to sqlite3_commit_hook()
** for the same database connection is overridden.
** ^the sqlite3_rollback_hook() interface registers a callback
** function to be invoked whenever a transaction is [rollback | rolled back].
** ^any callback set by a previous call to sqlite3_rollback_hook()
** for the same database connection is overridden.
** ^the parg argument is passed through to the callback.
** ^if the callback on a commit hook function returns non-zero,
** then the commit is converted into a rollback.
**
** ^the sqlite3_commit_hook(d,c,p) and sqlite3_rollback_hook(d,c,p) functions
** return the p argument from the previous call of the same function
** on the same [database connection] d, or null for
** the first call for each function on d.
**
** the commit and rollback hook callbacks are not reentrant.
** the callback implementation must not do anything that will modify
** the database connection that invoked the callback.  any actions
** to modify the database connection must be deferred until after the
** completion of the [sqlite3_step()] call that triggered the commit
** or rollback hook in the first place.
** note that running any other sql statements, including select statements,
** or merely calling [sqlite3_prepare_v2()] and [sqlite3_step()] will modify
** the database connections for the meaning of "modify" in this paragraph.
**
** ^registering a null function disables the callback.
**
** ^when the commit hook callback routine returns zero, the [commit]
** operation is allowed to continue normally.  ^if the commit hook
** returns non-zero, then the [commit] is converted into a [rollback].
** ^the rollback hook is invoked on a rollback that results from a commit
** hook returning non-zero, just as it would be with any other rollback.
**
** ^for the purposes of this api, a transaction is said to have been
** rolled back if an explicit "rollback" statement is executed, or
** an error or constraint causes an implicit rollback to occur.
** ^the rollback callback is not invoked if a transaction is
** automatically rolled back because the database connection is closed.
**
** see also the [sqlite3_update_hook()] interface.
*/
sqlite_api void *sqlite3_commit_hook(sqlite3*, int(*)(void*), void*);
sqlite_api void *sqlite3_rollback_hook(sqlite3*, void(*)(void *), void*);

/*
** capi3ref: data change notification callbacks
**
** ^the sqlite3_update_hook() interface registers a callback function
** with the [database connection] identified by the first argument
** to be invoked whenever a row is updated, inserted or deleted in
** a rowid table.
** ^any callback set by a previous call to this function
** for the same database connection is overridden.
**
** ^the second argument is a pointer to the function to invoke when a
** row is updated, inserted or deleted in a rowid table.
** ^the first argument to the callback is a copy of the third argument
** to sqlite3_update_hook().
** ^the second callback argument is one of [sqlite_insert], [sqlite_delete],
** or [sqlite_update], depending on the operation that caused the callback
** to be invoked.
** ^the third and fourth arguments to the callback contain pointers to the
** database and table name containing the affected row.
** ^the final callback parameter is the [rowid] of the row.
** ^in the case of an update, this is the [rowid] after the update takes place.
**
** ^(the update hook is not invoked when internal system tables are
** modified (i.e. sqlite_master and sqlite_sequence).)^
** ^the update hook is not invoked when [without rowid] tables are modified.
**
** ^in the current implementation, the update hook
** is not invoked when duplication rows are deleted because of an
** [on conflict | on conflict replace] clause.  ^nor is the update hook
** invoked when rows are deleted using the [truncate optimization].
** the exceptions defined in this paragraph might change in a future
** release of sqlite.
**
** the update hook implementation must not do anything that will modify
** the database connection that invoked the update hook.  any actions
** to modify the database connection must be deferred until after the
** completion of the [sqlite3_step()] call that triggered the update hook.
** note that [sqlite3_prepare_v2()] and [sqlite3_step()] both modify their
** database connections for the meaning of "modify" in this paragraph.
**
** ^the sqlite3_update_hook(d,c,p) function
** returns the p argument from the previous call
** on the same [database connection] d, or null for
** the first call on d.
**
** see also the [sqlite3_commit_hook()] and [sqlite3_rollback_hook()]
** interfaces.
*/
sqlite_api void *sqlite3_update_hook(
  sqlite3*, 
  void(*)(void *,int ,char const *,char const *,sqlite3_int64),
  void*
);

/*
** capi3ref: enable or disable shared pager cache
**
** ^(this routine enables or disables the sharing of the database cache
** and schema data structures between [database connection | connections]
** to the same database. sharing is enabled if the argument is true
** and disabled if the argument is false.)^
**
** ^cache sharing is enabled and disabled for an entire process.
** this is a change as of sqlite version 3.5.0. in prior versions of sqlite,
** sharing was enabled or disabled for each thread separately.
**
** ^(the cache sharing mode set by this interface effects all subsequent
** calls to [sqlite3_open()], [sqlite3_open_v2()], and [sqlite3_open16()].
** existing database connections continue use the sharing mode
** that was in effect at the time they were opened.)^
**
** ^(this routine returns [sqlite_ok] if shared cache was enabled or disabled
** successfully.  an [error code] is returned otherwise.)^
**
** ^shared cache is disabled by default. but this might change in
** future releases of sqlite.  applications that care about shared
** cache setting should set it explicitly.
**
** this interface is threadsafe on processors where writing a
** 32-bit integer is atomic.
**
** see also:  [sqlite shared-cache mode]
*/
sqlite_api int sqlite3_enable_shared_cache(int);

/*
** capi3ref: attempt to free heap memory
**
** ^the sqlite3_release_memory() interface attempts to free n bytes
** of heap memory by deallocating non-essential memory allocations
** held by the database library.   memory used to cache database
** pages to improve performance is an example of non-essential memory.
** ^sqlite3_release_memory() returns the number of bytes actually freed,
** which might be more or less than the amount requested.
** ^the sqlite3_release_memory() routine is a no-op returning zero
** if sqlite is not compiled with [sqlite_enable_memory_management].
**
** see also: [sqlite3_db_release_memory()]
*/
sqlite_api int sqlite3_release_memory(int);

/*
** capi3ref: free memory used by a database connection
**
** ^the sqlite3_db_release_memory(d) interface attempts to free as much heap
** memory as possible from database connection d. unlike the
** [sqlite3_release_memory()] interface, this interface is in effect even
** when the [sqlite_enable_memory_management] compile-time option is
** omitted.
**
** see also: [sqlite3_release_memory()]
*/
sqlite_api int sqlite3_db_release_memory(sqlite3*);

/*
** capi3ref: impose a limit on heap size
**
** ^the sqlite3_soft_heap_limit64() interface sets and/or queries the
** soft limit on the amount of heap memory that may be allocated by sqlite.
** ^sqlite strives to keep heap memory utilization below the soft heap
** limit by reducing the number of pages held in the page cache
** as heap memory usages approaches the limit.
** ^the soft heap limit is "soft" because even though sqlite strives to stay
** below the limit, it will exceed the limit rather than generate
** an [sqlite_nomem] error.  in other words, the soft heap limit 
** is advisory only.
**
** ^the return value from sqlite3_soft_heap_limit64() is the size of
** the soft heap limit prior to the call, or negative in the case of an
** error.  ^if the argument n is negative
** then no change is made to the soft heap limit.  hence, the current
** size of the soft heap limit can be determined by invoking
** sqlite3_soft_heap_limit64() with a negative argument.
**
** ^if the argument n is zero then the soft heap limit is disabled.
**
** ^(the soft heap limit is not enforced in the current implementation
** if one or more of following conditions are true:
**
** <ul>
** <li> the soft heap limit is set to zero.
** <li> memory accounting is disabled using a combination of the
**      [sqlite3_config]([sqlite_config_memstatus],...) start-time option and
**      the [sqlite_default_memstatus] compile-time option.
** <li> an alternative page cache implementation is specified using
**      [sqlite3_config]([sqlite_config_pcache2],...).
** <li> the page cache allocates from its own memory pool supplied
**      by [sqlite3_config]([sqlite_config_pagecache],...) rather than
**      from the heap.
** </ul>)^
**
** beginning with sqlite version 3.7.3, the soft heap limit is enforced
** regardless of whether or not the [sqlite_enable_memory_management]
** compile-time option is invoked.  with [sqlite_enable_memory_management],
** the soft heap limit is enforced on every memory allocation.  without
** [sqlite_enable_memory_management], the soft heap limit is only enforced
** when memory is allocated by the page cache.  testing suggests that because
** the page cache is the predominate memory user in sqlite, most
** applications will achieve adequate soft heap limit enforcement without
** the use of [sqlite_enable_memory_management].
**
** the circumstances under which sqlite will enforce the soft heap limit may
** changes in future releases of sqlite.
*/
sqlite_api sqlite3_int64 sqlite3_soft_heap_limit64(sqlite3_int64 n);

/*
** capi3ref: deprecated soft heap limit interface
** deprecated
**
** this is a deprecated version of the [sqlite3_soft_heap_limit64()]
** interface.  this routine is provided for historical compatibility
** only.  all new applications should use the
** [sqlite3_soft_heap_limit64()] interface rather than this one.
*/
sqlite_api sqlite_deprecated void sqlite3_soft_heap_limit(int n);


/*
** capi3ref: extract metadata about a column of a table
**
** ^this routine returns metadata about a specific column of a specific
** database table accessible using the [database connection] handle
** passed as the first function argument.
**
** ^the column is identified by the second, third and fourth parameters to
** this function. ^the second parameter is either the name of the database
** (i.e. "main", "temp", or an attached database) containing the specified
** table or null. ^if it is null, then all attached databases are searched
** for the table using the same algorithm used by the database engine to
** resolve unqualified table references.
**
** ^the third and fourth parameters to this function are the table and column
** name of the desired column, respectively. neither of these parameters
** may be null.
**
** ^metadata is returned by writing to the memory locations passed as the 5th
** and subsequent parameters to this function. ^any of these arguments may be
** null, in which case the corresponding element of metadata is omitted.
**
** ^(<blockquote>
** <table border="1">
** <tr><th> parameter <th> output<br>type <th>  description
**
** <tr><td> 5th <td> const char* <td> data type
** <tr><td> 6th <td> const char* <td> name of default collation sequence
** <tr><td> 7th <td> int         <td> true if column has a not null constraint
** <tr><td> 8th <td> int         <td> true if column is part of the primary key
** <tr><td> 9th <td> int         <td> true if column is [autoincrement]
** </table>
** </blockquote>)^
**
** ^the memory pointed to by the character pointers returned for the
** declaration type and collation sequence is valid only until the next
** call to any sqlite api function.
**
** ^if the specified table is actually a view, an [error code] is returned.
**
** ^if the specified column is "rowid", "oid" or "_rowid_" and an
** [integer primary key] column has been explicitly declared, then the output
** parameters are set for the explicitly declared column. ^(if there is no
** explicitly declared [integer primary key] column, then the output
** parameters are set as follows:
**
** <pre>
**     data type: "integer"
**     collation sequence: "binary"
**     not null: 0
**     primary key: 1
**     auto increment: 0
** </pre>)^
**
** ^(this function may load one or more schemas from database files. if an
** error occurs during this process, or if the requested table or column
** cannot be found, an [error code] is returned and an error message left
** in the [database connection] (to be retrieved using sqlite3_errmsg()).)^
**
** ^this api is only available if the library was compiled with the
** [sqlite_enable_column_metadata] c-preprocessor symbol defined.
*/
sqlite_api int sqlite3_table_column_metadata(
  sqlite3 *db,                /* connection handle */
  const char *zdbname,        /* database name or null */
  const char *ztablename,     /* table name */
  const char *zcolumnname,    /* column name */
  char const **pzdatatype,    /* output: declared data type */
  char const **pzcollseq,     /* output: collation sequence name */
  int *pnotnull,              /* output: true if not null constraint exists */
  int *pprimarykey,           /* output: true if column part of pk */
  int *pautoinc               /* output: true if column is auto-increment */
);

/*
** capi3ref: load an extension
**
** ^this interface loads an sqlite extension library from the named file.
**
** ^the sqlite3_load_extension() interface attempts to load an
** [sqlite extension] library contained in the file zfile.  if
** the file cannot be loaded directly, attempts are made to load
** with various operating-system specific extensions added.
** so for example, if "samplelib" cannot be loaded, then names like
** "samplelib.so" or "samplelib.dylib" or "samplelib.dll" might
** be tried also.
**
** ^the entry point is zproc.
** ^(zproc may be 0, in which case sqlite will try to come up with an
** entry point name on its own.  it first tries "sqlite3_extension_init".
** if that does not work, it constructs a name "sqlite3_x_init" where the
** x is consists of the lower-case equivalent of all ascii alphabetic
** characters in the filename from the last "/" to the first following
** "." and omitting any initial "lib".)^
** ^the sqlite3_load_extension() interface returns
** [sqlite_ok] on success and [sqlite_error] if something goes wrong.
** ^if an error occurs and pzerrmsg is not 0, then the
** [sqlite3_load_extension()] interface shall attempt to
** fill *pzerrmsg with error message text stored in memory
** obtained from [sqlite3_malloc()]. the calling function
** should free this memory by calling [sqlite3_free()].
**
** ^extension loading must be enabled using
** [sqlite3_enable_load_extension()] prior to calling this api,
** otherwise an error will be returned.
**
** see also the [load_extension() sql function].
*/
sqlite_api int sqlite3_load_extension(
  sqlite3 *db,          /* load the extension into this database connection */
  const char *zfile,    /* name of the shared library containing extension */
  const char *zproc,    /* entry point.  derived from zfile if 0 */
  char **pzerrmsg       /* put error message here if not 0 */
);

/*
** capi3ref: enable or disable extension loading
**
** ^so as not to open security holes in older applications that are
** unprepared to deal with [extension loading], and as a means of disabling
** [extension loading] while evaluating user-entered sql, the following api
** is provided to turn the [sqlite3_load_extension()] mechanism on and off.
**
** ^extension loading is off by default.
** ^call the sqlite3_enable_load_extension() routine with onoff==1
** to turn extension loading on and call it with onoff==0 to turn
** it back off again.
*/
sqlite_api int sqlite3_enable_load_extension(sqlite3 *db, int onoff);

/*
** capi3ref: automatically load statically linked extensions
**
** ^this interface causes the xentrypoint() function to be invoked for
** each new [database connection] that is created.  the idea here is that
** xentrypoint() is the entry point for a statically linked [sqlite extension]
** that is to be automatically loaded into all new database connections.
**
** ^(even though the function prototype shows that xentrypoint() takes
** no arguments and returns void, sqlite invokes xentrypoint() with three
** arguments and expects and integer result as if the signature of the
** entry point where as follows:
**
** <blockquote><pre>
** &nbsp;  int xentrypoint(
** &nbsp;    sqlite3 *db,
** &nbsp;    const char **pzerrmsg,
** &nbsp;    const struct sqlite3_api_routines *pthunk
** &nbsp;  );
** </pre></blockquote>)^
**
** if the xentrypoint routine encounters an error, it should make *pzerrmsg
** point to an appropriate error message (obtained from [sqlite3_mprintf()])
** and return an appropriate [error code].  ^sqlite ensures that *pzerrmsg
** is null before calling the xentrypoint().  ^sqlite will invoke
** [sqlite3_free()] on *pzerrmsg after xentrypoint() returns.  ^if any
** xentrypoint() returns an error, the [sqlite3_open()], [sqlite3_open16()],
** or [sqlite3_open_v2()] call that provoked the xentrypoint() will fail.
**
** ^calling sqlite3_auto_extension(x) with an entry point x that is already
** on the list of automatic extensions is a harmless no-op. ^no entry point
** will be called more than once for each database connection that is opened.
**
** see also: [sqlite3_reset_auto_extension()]
** and [sqlite3_cancel_auto_extension()]
*/
sqlite_api int sqlite3_auto_extension(void (*xentrypoint)(void));

/*
** capi3ref: cancel automatic extension loading
**
** ^the [sqlite3_cancel_auto_extension(x)] interface unregisters the
** initialization routine x that was registered using a prior call to
** [sqlite3_auto_extension(x)].  ^the [sqlite3_cancel_auto_extension(x)]
** routine returns 1 if initialization routine x was successfully 
** unregistered and it returns 0 if x was not on the list of initialization
** routines.
*/
sqlite_api int sqlite3_cancel_auto_extension(void (*xentrypoint)(void));

/*
** capi3ref: reset automatic extension loading
**
** ^this interface disables all automatic extensions previously
** registered using [sqlite3_auto_extension()].
*/
sqlite_api void sqlite3_reset_auto_extension(void);

/*
** the interface to the virtual-table mechanism is currently considered
** to be experimental.  the interface might change in incompatible ways.
** if this is a problem for you, do not use the interface at this time.
**
** when the virtual-table mechanism stabilizes, we will declare the
** interface fixed, support it indefinitely, and remove this comment.
*/

/*
** structures used by the virtual table interface
*/
typedef struct sqlite3_vtab sqlite3_vtab;
typedef struct sqlite3_index_info sqlite3_index_info;
typedef struct sqlite3_vtab_cursor sqlite3_vtab_cursor;
typedef struct sqlite3_module sqlite3_module;

/*
** capi3ref: virtual table object
** keywords: sqlite3_module {virtual table module}
**
** this structure, sometimes called a "virtual table module", 
** defines the implementation of a [virtual tables].  
** this structure consists mostly of methods for the module.
**
** ^a virtual table module is created by filling in a persistent
** instance of this structure and passing a pointer to that instance
** to [sqlite3_create_module()] or [sqlite3_create_module_v2()].
** ^the registration remains valid until it is replaced by a different
** module or until the [database connection] closes.  the content
** of this structure must not change while it is registered with
** any database connection.
*/
struct sqlite3_module {
  int iversion;
  int (*xcreate)(sqlite3*, void *paux,
               int argc, const char *const*argv,
               sqlite3_vtab **ppvtab, char**);
  int (*xconnect)(sqlite3*, void *paux,
               int argc, const char *const*argv,
               sqlite3_vtab **ppvtab, char**);
  int (*xbestindex)(sqlite3_vtab *pvtab, sqlite3_index_info*);
  int (*xdisconnect)(sqlite3_vtab *pvtab);
  int (*xdestroy)(sqlite3_vtab *pvtab);
  int (*xopen)(sqlite3_vtab *pvtab, sqlite3_vtab_cursor **ppcursor);
  int (*xclose)(sqlite3_vtab_cursor*);
  int (*xfilter)(sqlite3_vtab_cursor*, int idxnum, const char *idxstr,
                int argc, sqlite3_value **argv);
  int (*xnext)(sqlite3_vtab_cursor*);
  int (*xeof)(sqlite3_vtab_cursor*);
  int (*xcolumn)(sqlite3_vtab_cursor*, sqlite3_context*, int);
  int (*xrowid)(sqlite3_vtab_cursor*, sqlite3_int64 *prowid);
  int (*xupdate)(sqlite3_vtab *, int, sqlite3_value **, sqlite3_int64 *);
  int (*xbegin)(sqlite3_vtab *pvtab);
  int (*xsync)(sqlite3_vtab *pvtab);
  int (*xcommit)(sqlite3_vtab *pvtab);
  int (*xrollback)(sqlite3_vtab *pvtab);
  int (*xfindfunction)(sqlite3_vtab *pvtab, int narg, const char *zname,
                       void (**pxfunc)(sqlite3_context*,int,sqlite3_value**),
                       void **pparg);
  int (*xrename)(sqlite3_vtab *pvtab, const char *znew);
  /* the methods above are in version 1 of the sqlite_module object. those 
  ** below are for version 2 and greater. */
  int (*xsavepoint)(sqlite3_vtab *pvtab, int);
  int (*xrelease)(sqlite3_vtab *pvtab, int);
  int (*xrollbackto)(sqlite3_vtab *pvtab, int);
};

/*
** capi3ref: virtual table indexing information
** keywords: sqlite3_index_info
**
** the sqlite3_index_info structure and its substructures is used as part
** of the [virtual table] interface to
** pass information into and receive the reply from the [xbestindex]
** method of a [virtual table module].  the fields under **inputs** are the
** inputs to xbestindex and are read-only.  xbestindex inserts its
** results into the **outputs** fields.
**
** ^(the aconstraint[] array records where clause constraints of the form:
**
** <blockquote>column op expr</blockquote>
**
** where op is =, &lt;, &lt;=, &gt;, or &gt;=.)^  ^(the particular operator is
** stored in aconstraint[].op using one of the
** [sqlite_index_constraint_eq | sqlite_index_constraint_ values].)^
** ^(the index of the column is stored in
** aconstraint[].icolumn.)^  ^(aconstraint[].usable is true if the
** expr on the right-hand side can be evaluated (and thus the constraint
** is usable) and false if it cannot.)^
**
** ^the optimizer automatically inverts terms of the form "expr op column"
** and makes other simplifications to the where clause in an attempt to
** get as many where clause terms into the form shown above as possible.
** ^the aconstraint[] array only reports where clause terms that are
** relevant to the particular virtual table being queried.
**
** ^information about the order by clause is stored in aorderby[].
** ^each term of aorderby records a column of the order by clause.
**
** the [xbestindex] method must fill aconstraintusage[] with information
** about what parameters to pass to xfilter.  ^if argvindex>0 then
** the right-hand side of the corresponding aconstraint[] is evaluated
** and becomes the argvindex-th entry in argv.  ^(if aconstraintusage[].omit
** is true, then the constraint is assumed to be fully handled by the
** virtual table and is not checked again by sqlite.)^
**
** ^the idxnum and idxptr values are recorded and passed into the
** [xfilter] method.
** ^[sqlite3_free()] is used to free idxptr if and only if
** needtofreeidxptr is true.
**
** ^the orderbyconsumed means that output from [xfilter]/[xnext] will occur in
** the correct order to satisfy the order by clause so that no separate
** sorting step is required.
**
** ^the estimatedcost value is an estimate of the cost of a particular
** strategy. a cost of n indicates that the cost of the strategy is similar
** to a linear scan of an sqlite table with n rows. a cost of log(n) 
** indicates that the expense of the operation is similar to that of a
** binary search on a unique indexed field of an sqlite table with n rows.
**
** ^the estimatedrows value is an estimate of the number of rows that
** will be returned by the strategy.
**
** important: the estimatedrows field was added to the sqlite3_index_info
** structure for sqlite version 3.8.2. if a virtual table extension is
** used with an sqlite version earlier than 3.8.2, the results of attempting 
** to read or write the estimatedrows field are undefined (but are likely 
** to included crashing the application). the estimatedrows field should
** therefore only be used if [sqlite3_libversion_number()] returns a
** value greater than or equal to 3008002.
*/
struct sqlite3_index_info {
  /* inputs */
  int nconstraint;           /* number of entries in aconstraint */
  struct sqlite3_index_constraint {
     int icolumn;              /* column on left-hand side of constraint */
     unsigned char op;         /* constraint operator */
     unsigned char usable;     /* true if this constraint is usable */
     int itermoffset;          /* used internally - xbestindex should ignore */
  } *aconstraint;            /* table of where clause constraints */
  int norderby;              /* number of terms in the order by clause */
  struct sqlite3_index_orderby {
     int icolumn;              /* column number */
     unsigned char desc;       /* true for desc.  false for asc. */
  } *aorderby;               /* the order by clause */
  /* outputs */
  struct sqlite3_index_constraint_usage {
    int argvindex;           /* if >0, constraint is part of argv to xfilter */
    unsigned char omit;      /* do not code a test for this constraint */
  } *aconstraintusage;
  int idxnum;                /* number used to identify the index */
  char *idxstr;              /* string, possibly obtained from sqlite3_malloc */
  int needtofreeidxstr;      /* free idxstr using sqlite3_free() if true */
  int orderbyconsumed;       /* true if output is already ordered */
  double estimatedcost;           /* estimated cost of using this index */
  /* fields below are only available in sqlite 3.8.2 and later */
  sqlite3_int64 estimatedrows;    /* estimated number of rows returned */
};

/*
** capi3ref: virtual table constraint operator codes
**
** these macros defined the allowed values for the
** [sqlite3_index_info].aconstraint[].op field.  each value represents
** an operator that is part of a constraint term in the where clause of
** a query that uses a [virtual table].
*/
#define sqlite_index_constraint_eq    2
#define sqlite_index_constraint_gt    4
#define sqlite_index_constraint_le    8
#define sqlite_index_constraint_lt    16
#define sqlite_index_constraint_ge    32
#define sqlite_index_constraint_match 64

/*
** capi3ref: register a virtual table implementation
**
** ^these routines are used to register a new [virtual table module] name.
** ^module names must be registered before
** creating a new [virtual table] using the module and before using a
** preexisting [virtual table] for the module.
**
** ^the module name is registered on the [database connection] specified
** by the first parameter.  ^the name of the module is given by the 
** second parameter.  ^the third parameter is a pointer to
** the implementation of the [virtual table module].   ^the fourth
** parameter is an arbitrary client data pointer that is passed through
** into the [xcreate] and [xconnect] methods of the virtual table module
** when a new virtual table is be being created or reinitialized.
**
** ^the sqlite3_create_module_v2() interface has a fifth parameter which
** is a pointer to a destructor for the pclientdata.  ^sqlite will
** invoke the destructor function (if it is not null) when sqlite
** no longer needs the pclientdata pointer.  ^the destructor will also
** be invoked if the call to sqlite3_create_module_v2() fails.
** ^the sqlite3_create_module()
** interface is equivalent to sqlite3_create_module_v2() with a null
** destructor.
*/
sqlite_api int sqlite3_create_module(
  sqlite3 *db,               /* sqlite connection to register module with */
  const char *zname,         /* name of the module */
  const sqlite3_module *p,   /* methods for the module */
  void *pclientdata          /* client data for xcreate/xconnect */
);
sqlite_api int sqlite3_create_module_v2(
  sqlite3 *db,               /* sqlite connection to register module with */
  const char *zname,         /* name of the module */
  const sqlite3_module *p,   /* methods for the module */
  void *pclientdata,         /* client data for xcreate/xconnect */
  void(*xdestroy)(void*)     /* module destructor function */
);

/*
** capi3ref: virtual table instance object
** keywords: sqlite3_vtab
**
** every [virtual table module] implementation uses a subclass
** of this object to describe a particular instance
** of the [virtual table].  each subclass will
** be tailored to the specific needs of the module implementation.
** the purpose of this superclass is to define certain fields that are
** common to all module implementations.
**
** ^virtual tables methods can set an error message by assigning a
** string obtained from [sqlite3_mprintf()] to zerrmsg.  the method should
** take care that any prior string is freed by a call to [sqlite3_free()]
** prior to assigning a new string to zerrmsg.  ^after the error message
** is delivered up to the client application, the string will be automatically
** freed by sqlite3_free() and the zerrmsg field will be zeroed.
*/
struct sqlite3_vtab {
  const sqlite3_module *pmodule;  /* the module for this virtual table */
  int nref;                       /* no longer used */
  char *zerrmsg;                  /* error message from sqlite3_mprintf() */
  /* virtual table implementations will typically add additional fields */
};

/*
** capi3ref: virtual table cursor object
** keywords: sqlite3_vtab_cursor {virtual table cursor}
**
** every [virtual table module] implementation uses a subclass of the
** following structure to describe cursors that point into the
** [virtual table] and are used
** to loop through the virtual table.  cursors are created using the
** [sqlite3_module.xopen | xopen] method of the module and are destroyed
** by the [sqlite3_module.xclose | xclose] method.  cursors are used
** by the [xfilter], [xnext], [xeof], [xcolumn], and [xrowid] methods
** of the module.  each module implementation will define
** the content of a cursor structure to suit its own needs.
**
** this superclass exists in order to define fields of the cursor that
** are common to all implementations.
*/
struct sqlite3_vtab_cursor {
  sqlite3_vtab *pvtab;      /* virtual table of this cursor */
  /* virtual table implementations will typically add additional fields */
};

/*
** capi3ref: declare the schema of a virtual table
**
** ^the [xcreate] and [xconnect] methods of a
** [virtual table module] call this interface
** to declare the format (the names and datatypes of the columns) of
** the virtual tables they implement.
*/
sqlite_api int sqlite3_declare_vtab(sqlite3*, const char *zsql);

/*
** capi3ref: overload a function for a virtual table
**
** ^(virtual tables can provide alternative implementations of functions
** using the [xfindfunction] method of the [virtual table module].  
** but global versions of those functions
** must exist in order to be overloaded.)^
**
** ^(this api makes sure a global version of a function with a particular
** name and number of parameters exists.  if no such function exists
** before this api is called, a new function is created.)^  ^the implementation
** of the new function always causes an exception to be thrown.  so
** the new function is not good for anything by itself.  its only
** purpose is to be a placeholder function that can be overloaded
** by a [virtual table].
*/
sqlite_api int sqlite3_overload_function(sqlite3*, const char *zfuncname, int narg);

/*
** the interface to the virtual-table mechanism defined above (back up
** to a comment remarkably similar to this one) is currently considered
** to be experimental.  the interface might change in incompatible ways.
** if this is a problem for you, do not use the interface at this time.
**
** when the virtual-table mechanism stabilizes, we will declare the
** interface fixed, support it indefinitely, and remove this comment.
*/

/*
** capi3ref: a handle to an open blob
** keywords: {blob handle} {blob handles}
**
** an instance of this object represents an open blob on which
** [sqlite3_blob_open | incremental blob i/o] can be performed.
** ^objects of this type are created by [sqlite3_blob_open()]
** and destroyed by [sqlite3_blob_close()].
** ^the [sqlite3_blob_read()] and [sqlite3_blob_write()] interfaces
** can be used to read or write small subsections of the blob.
** ^the [sqlite3_blob_bytes()] interface returns the size of the blob in bytes.
*/
typedef struct sqlite3_blob sqlite3_blob;

/*
** capi3ref: open a blob for incremental i/o
**
** ^(this interfaces opens a [blob handle | handle] to the blob located
** in row irow, column zcolumn, table ztable in database zdb;
** in other words, the same blob that would be selected by:
**
** <pre>
**     select zcolumn from zdb.ztable where [rowid] = irow;
** </pre>)^
**
** ^if the flags parameter is non-zero, then the blob is opened for read
** and write access. ^if it is zero, the blob is opened for read access.
** ^it is not possible to open a column that is part of an index or primary 
** key for writing. ^if [foreign key constraints] are enabled, it is 
** not possible to open a column that is part of a [child key] for writing.
**
** ^note that the database name is not the filename that contains
** the database but rather the symbolic name of the database that
** appears after the as keyword when the database is connected using [attach].
** ^for the main database file, the database name is "main".
** ^for temp tables, the database name is "temp".
**
** ^(on success, [sqlite_ok] is returned and the new [blob handle] is written
** to *ppblob. otherwise an [error code] is returned and *ppblob is set
** to be a null pointer.)^
** ^this function sets the [database connection] error code and message
** accessible via [sqlite3_errcode()] and [sqlite3_errmsg()] and related
** functions. ^note that the *ppblob variable is always initialized in a
** way that makes it safe to invoke [sqlite3_blob_close()] on *ppblob
** regardless of the success or failure of this routine.
**
** ^(if the row that a blob handle points to is modified by an
** [update], [delete], or by [on conflict] side-effects
** then the blob handle is marked as "expired".
** this is true if any column of the row is changed, even a column
** other than the one the blob handle is open on.)^
** ^calls to [sqlite3_blob_read()] and [sqlite3_blob_write()] for
** an expired blob handle fail with a return code of [sqlite_abort].
** ^(changes written into a blob prior to the blob expiring are not
** rolled back by the expiration of the blob.  such changes will eventually
** commit if the transaction continues to completion.)^
**
** ^use the [sqlite3_blob_bytes()] interface to determine the size of
** the opened blob.  ^the size of a blob may not be changed by this
** interface.  use the [update] sql command to change the size of a
** blob.
**
** ^the [sqlite3_blob_open()] interface will fail for a [without rowid]
** table.  incremental blob i/o is not possible on [without rowid] tables.
**
** ^the [sqlite3_bind_zeroblob()] and [sqlite3_result_zeroblob()] interfaces
** and the built-in [zeroblob] sql function can be used, if desired,
** to create an empty, zero-filled blob in which to read or write using
** this interface.
**
** to avoid a resource leak, every open [blob handle] should eventually
** be released by a call to [sqlite3_blob_close()].
*/
sqlite_api int sqlite3_blob_open(
  sqlite3*,
  const char *zdb,
  const char *ztable,
  const char *zcolumn,
  sqlite3_int64 irow,
  int flags,
  sqlite3_blob **ppblob
);

/*
** capi3ref: move a blob handle to a new row
**
** ^this function is used to move an existing blob handle so that it points
** to a different row of the same database table. ^the new row is identified
** by the rowid value passed as the second argument. only the row can be
** changed. ^the database, table and column on which the blob handle is open
** remain the same. moving an existing blob handle to a new row can be
** faster than closing the existing handle and opening a new one.
**
** ^(the new row must meet the same criteria as for [sqlite3_blob_open()] -
** it must exist and there must be either a blob or text value stored in
** the nominated column.)^ ^if the new row is not present in the table, or if
** it does not contain a blob or text value, or if another error occurs, an
** sqlite error code is returned and the blob handle is considered aborted.
** ^all subsequent calls to [sqlite3_blob_read()], [sqlite3_blob_write()] or
** [sqlite3_blob_reopen()] on an aborted blob handle immediately return
** sqlite_abort. ^calling [sqlite3_blob_bytes()] on an aborted blob handle
** always returns zero.
**
** ^this function sets the database handle error code and message.
*/
sqlite_api sqlite_experimental int sqlite3_blob_reopen(sqlite3_blob *, sqlite3_int64);

/*
** capi3ref: close a blob handle
**
** ^closes an open [blob handle].
**
** ^closing a blob shall cause the current transaction to commit
** if there are no other blobs, no pending prepared statements, and the
** database connection is in [autocommit mode].
** ^if any writes were made to the blob, they might be held in cache
** until the close operation if they will fit.
**
** ^(closing the blob often forces the changes
** out to disk and so if any i/o errors occur, they will likely occur
** at the time when the blob is closed.  any errors that occur during
** closing are reported as a non-zero return value.)^
**
** ^(the blob is closed unconditionally.  even if this routine returns
** an error code, the blob is still closed.)^
**
** ^calling this routine with a null pointer (such as would be returned
** by a failed call to [sqlite3_blob_open()]) is a harmless no-op.
*/
sqlite_api int sqlite3_blob_close(sqlite3_blob *);

/*
** capi3ref: return the size of an open blob
**
** ^returns the size in bytes of the blob accessible via the 
** successfully opened [blob handle] in its only argument.  ^the
** incremental blob i/o routines can only read or overwriting existing
** blob content; they cannot change the size of a blob.
**
** this routine only works on a [blob handle] which has been created
** by a prior successful call to [sqlite3_blob_open()] and which has not
** been closed by [sqlite3_blob_close()].  passing any other pointer in
** to this routine results in undefined and probably undesirable behavior.
*/
sqlite_api int sqlite3_blob_bytes(sqlite3_blob *);

/*
** capi3ref: read data from a blob incrementally
**
** ^(this function is used to read data from an open [blob handle] into a
** caller-supplied buffer. n bytes of data are copied into buffer z
** from the open blob, starting at offset ioffset.)^
**
** ^if offset ioffset is less than n bytes from the end of the blob,
** [sqlite_error] is returned and no data is read.  ^if n or ioffset is
** less than zero, [sqlite_error] is returned and no data is read.
** ^the size of the blob (and hence the maximum value of n+ioffset)
** can be determined using the [sqlite3_blob_bytes()] interface.
**
** ^an attempt to read from an expired [blob handle] fails with an
** error code of [sqlite_abort].
**
** ^(on success, sqlite3_blob_read() returns sqlite_ok.
** otherwise, an [error code] or an [extended error code] is returned.)^
**
** this routine only works on a [blob handle] which has been created
** by a prior successful call to [sqlite3_blob_open()] and which has not
** been closed by [sqlite3_blob_close()].  passing any other pointer in
** to this routine results in undefined and probably undesirable behavior.
**
** see also: [sqlite3_blob_write()].
*/
sqlite_api int sqlite3_blob_read(sqlite3_blob *, void *z, int n, int ioffset);

/*
** capi3ref: write data into a blob incrementally
**
** ^this function is used to write data into an open [blob handle] from a
** caller-supplied buffer. ^n bytes of data are copied from the buffer z
** into the open blob, starting at offset ioffset.
**
** ^if the [blob handle] passed as the first argument was not opened for
** writing (the flags parameter to [sqlite3_blob_open()] was zero),
** this function returns [sqlite_readonly].
**
** ^this function may only modify the contents of the blob; it is
** not possible to increase the size of a blob using this api.
** ^if offset ioffset is less than n bytes from the end of the blob,
** [sqlite_error] is returned and no data is written.  ^if n is
** less than zero [sqlite_error] is returned and no data is written.
** the size of the blob (and hence the maximum value of n+ioffset)
** can be determined using the [sqlite3_blob_bytes()] interface.
**
** ^an attempt to write to an expired [blob handle] fails with an
** error code of [sqlite_abort].  ^writes to the blob that occurred
** before the [blob handle] expired are not rolled back by the
** expiration of the handle, though of course those changes might
** have been overwritten by the statement that expired the blob handle
** or by other independent statements.
**
** ^(on success, sqlite3_blob_write() returns sqlite_ok.
** otherwise, an  [error code] or an [extended error code] is returned.)^
**
** this routine only works on a [blob handle] which has been created
** by a prior successful call to [sqlite3_blob_open()] and which has not
** been closed by [sqlite3_blob_close()].  passing any other pointer in
** to this routine results in undefined and probably undesirable behavior.
**
** see also: [sqlite3_blob_read()].
*/
sqlite_api int sqlite3_blob_write(sqlite3_blob *, const void *z, int n, int ioffset);

/*
** capi3ref: virtual file system objects
**
** a virtual filesystem (vfs) is an [sqlite3_vfs] object
** that sqlite uses to interact
** with the underlying operating system.  most sqlite builds come with a
** single default vfs that is appropriate for the host computer.
** new vfses can be registered and existing vfses can be unregistered.
** the following interfaces are provided.
**
** ^the sqlite3_vfs_find() interface returns a pointer to a vfs given its name.
** ^names are case sensitive.
** ^names are zero-terminated utf-8 strings.
** ^if there is no match, a null pointer is returned.
** ^if zvfsname is null then the default vfs is returned.
**
** ^new vfses are registered with sqlite3_vfs_register().
** ^each new vfs becomes the default vfs if the makedflt flag is set.
** ^the same vfs can be registered multiple times without injury.
** ^to make an existing vfs into the default vfs, register it again
** with the makedflt flag set.  if two different vfses with the
** same name are registered, the behavior is undefined.  if a
** vfs is registered with a name that is null or an empty string,
** then the behavior is undefined.
**
** ^unregister a vfs with the sqlite3_vfs_unregister() interface.
** ^(if the default vfs is unregistered, another vfs is chosen as
** the default.  the choice for the new vfs is arbitrary.)^
*/
sqlite_api sqlite3_vfs *sqlite3_vfs_find(const char *zvfsname);
sqlite_api int sqlite3_vfs_register(sqlite3_vfs*, int makedflt);
sqlite_api int sqlite3_vfs_unregister(sqlite3_vfs*);

/*
** capi3ref: mutexes
**
** the sqlite core uses these routines for thread
** synchronization. though they are intended for internal
** use by sqlite, code that links against sqlite is
** permitted to use any of these routines.
**
** the sqlite source code contains multiple implementations
** of these mutex routines.  an appropriate implementation
** is selected automatically at compile-time.  ^(the following
** implementations are available in the sqlite core:
**
** <ul>
** <li>   sqlite_mutex_pthreads
** <li>   sqlite_mutex_w32
** <li>   sqlite_mutex_noop
** </ul>)^
**
** ^the sqlite_mutex_noop implementation is a set of routines
** that does no real locking and is appropriate for use in
** a single-threaded application.  ^the sqlite_mutex_pthreads and
** sqlite_mutex_w32 implementations are appropriate for use on unix
** and windows.
**
** ^(if sqlite is compiled with the sqlite_mutex_appdef preprocessor
** macro defined (with "-dsqlite_mutex_appdef=1"), then no mutex
** implementation is included with the library. in this case the
** application must supply a custom mutex implementation using the
** [sqlite_config_mutex] option of the sqlite3_config() function
** before calling sqlite3_initialize() or any other public sqlite3_
** function that calls sqlite3_initialize().)^
**
** ^the sqlite3_mutex_alloc() routine allocates a new
** mutex and returns a pointer to it. ^if it returns null
** that means that a mutex could not be allocated.  ^sqlite
** will unwind its stack and return an error.  ^(the argument
** to sqlite3_mutex_alloc() is one of these integer constants:
**
** <ul>
** <li>  sqlite_mutex_fast
** <li>  sqlite_mutex_recursive
** <li>  sqlite_mutex_static_master
** <li>  sqlite_mutex_static_mem
** <li>  sqlite_mutex_static_open
** <li>  sqlite_mutex_static_prng
** <li>  sqlite_mutex_static_lru
** <li>  sqlite_mutex_static_pmem
** <li>  sqlite_mutex_static_app1
** <li>  sqlite_mutex_static_app2
** </ul>)^
**
** ^the first two constants (sqlite_mutex_fast and sqlite_mutex_recursive)
** cause sqlite3_mutex_alloc() to create
** a new mutex.  ^the new mutex is recursive when sqlite_mutex_recursive
** is used but not necessarily so when sqlite_mutex_fast is used.
** the mutex implementation does not need to make a distinction
** between sqlite_mutex_recursive and sqlite_mutex_fast if it does
** not want to.  ^sqlite will only request a recursive mutex in
** cases where it really needs one.  ^if a faster non-recursive mutex
** implementation is available on the host platform, the mutex subsystem
** might return such a mutex in response to sqlite_mutex_fast.
**
** ^the other allowed parameters to sqlite3_mutex_alloc() (anything other
** than sqlite_mutex_fast and sqlite_mutex_recursive) each return
** a pointer to a static preexisting mutex.  ^six static mutexes are
** used by the current version of sqlite.  future versions of sqlite
** may add additional static mutexes.  static mutexes are for internal
** use by sqlite only.  applications that use sqlite mutexes should
** use only the dynamic mutexes returned by sqlite_mutex_fast or
** sqlite_mutex_recursive.
**
** ^note that if one of the dynamic mutex parameters (sqlite_mutex_fast
** or sqlite_mutex_recursive) is used then sqlite3_mutex_alloc()
** returns a different mutex on every call.  ^but for the static
** mutex types, the same mutex is returned on every call that has
** the same type number.
**
** ^the sqlite3_mutex_free() routine deallocates a previously
** allocated dynamic mutex.  ^sqlite is careful to deallocate every
** dynamic mutex that it allocates.  the dynamic mutexes must not be in
** use when they are deallocated.  attempting to deallocate a static
** mutex results in undefined behavior.  ^sqlite never deallocates
** a static mutex.
**
** ^the sqlite3_mutex_enter() and sqlite3_mutex_try() routines attempt
** to enter a mutex.  ^if another thread is already within the mutex,
** sqlite3_mutex_enter() will block and sqlite3_mutex_try() will return
** sqlite_busy.  ^the sqlite3_mutex_try() interface returns [sqlite_ok]
** upon successful entry.  ^(mutexes created using
** sqlite_mutex_recursive can be entered multiple times by the same thread.
** in such cases the,
** mutex must be exited an equal number of times before another thread
** can enter.)^  ^(if the same thread tries to enter any other
** kind of mutex more than once, the behavior is undefined.
** sqlite will never exhibit
** such behavior in its own use of mutexes.)^
**
** ^(some systems (for example, windows 95) do not support the operation
** implemented by sqlite3_mutex_try().  on those systems, sqlite3_mutex_try()
** will always return sqlite_busy.  the sqlite core only ever uses
** sqlite3_mutex_try() as an optimization so this is acceptable behavior.)^
**
** ^the sqlite3_mutex_leave() routine exits a mutex that was
** previously entered by the same thread.   ^(the behavior
** is undefined if the mutex is not currently entered by the
** calling thread or is not currently allocated.  sqlite will
** never do either.)^
**
** ^if the argument to sqlite3_mutex_enter(), sqlite3_mutex_try(), or
** sqlite3_mutex_leave() is a null pointer, then all three routines
** behave as no-ops.
**
** see also: [sqlite3_mutex_held()] and [sqlite3_mutex_notheld()].
*/
sqlite_api sqlite3_mutex *sqlite3_mutex_alloc(int);
sqlite_api void sqlite3_mutex_free(sqlite3_mutex*);
sqlite_api void sqlite3_mutex_enter(sqlite3_mutex*);
sqlite_api int sqlite3_mutex_try(sqlite3_mutex*);
sqlite_api void sqlite3_mutex_leave(sqlite3_mutex*);

/*
** capi3ref: mutex methods object
**
** an instance of this structure defines the low-level routines
** used to allocate and use mutexes.
**
** usually, the default mutex implementations provided by sqlite are
** sufficient, however the user has the option of substituting a custom
** implementation for specialized deployments or systems for which sqlite
** does not provide a suitable implementation. in this case, the user
** creates and populates an instance of this structure to pass
** to sqlite3_config() along with the [sqlite_config_mutex] option.
** additionally, an instance of this structure can be used as an
** output variable when querying the system for the current mutex
** implementation, using the [sqlite_config_getmutex] option.
**
** ^the xmutexinit method defined by this structure is invoked as
** part of system initialization by the sqlite3_initialize() function.
** ^the xmutexinit routine is called by sqlite exactly once for each
** effective call to [sqlite3_initialize()].
**
** ^the xmutexend method defined by this structure is invoked as
** part of system shutdown by the sqlite3_shutdown() function. the
** implementation of this method is expected to release all outstanding
** resources obtained by the mutex methods implementation, especially
** those obtained by the xmutexinit method.  ^the xmutexend()
** interface is invoked exactly once for each call to [sqlite3_shutdown()].
**
** ^(the remaining seven methods defined by this structure (xmutexalloc,
** xmutexfree, xmutexenter, xmutextry, xmutexleave, xmutexheld and
** xmutexnotheld) implement the following interfaces (respectively):
**
** <ul>
**   <li>  [sqlite3_mutex_alloc()] </li>
**   <li>  [sqlite3_mutex_free()] </li>
**   <li>  [sqlite3_mutex_enter()] </li>
**   <li>  [sqlite3_mutex_try()] </li>
**   <li>  [sqlite3_mutex_leave()] </li>
**   <li>  [sqlite3_mutex_held()] </li>
**   <li>  [sqlite3_mutex_notheld()] </li>
** </ul>)^
**
** the only difference is that the public sqlite3_xxx functions enumerated
** above silently ignore any invocations that pass a null pointer instead
** of a valid mutex handle. the implementations of the methods defined
** by this structure are not required to handle this case, the results
** of passing a null pointer instead of a valid mutex handle are undefined
** (i.e. it is acceptable to provide an implementation that segfaults if
** it is passed a null pointer).
**
** the xmutexinit() method must be threadsafe.  ^it must be harmless to
** invoke xmutexinit() multiple times within the same process and without
** intervening calls to xmutexend().  second and subsequent calls to
** xmutexinit() must be no-ops.
**
** ^xmutexinit() must not use sqlite memory allocation ([sqlite3_malloc()]
** and its associates).  ^similarly, xmutexalloc() must not use sqlite memory
** allocation for a static mutex.  ^however xmutexalloc() may use sqlite
** memory allocation for a fast or recursive mutex.
**
** ^sqlite will invoke the xmutexend() method when [sqlite3_shutdown()] is
** called, but only if the prior call to xmutexinit returned sqlite_ok.
** if xmutexinit fails in any way, it is expected to clean up after itself
** prior to returning.
*/
typedef struct sqlite3_mutex_methods sqlite3_mutex_methods;
struct sqlite3_mutex_methods {
  int (*xmutexinit)(void);
  int (*xmutexend)(void);
  sqlite3_mutex *(*xmutexalloc)(int);
  void (*xmutexfree)(sqlite3_mutex *);
  void (*xmutexenter)(sqlite3_mutex *);
  int (*xmutextry)(sqlite3_mutex *);
  void (*xmutexleave)(sqlite3_mutex *);
  int (*xmutexheld)(sqlite3_mutex *);
  int (*xmutexnotheld)(sqlite3_mutex *);
};

/*
** capi3ref: mutex verification routines
**
** the sqlite3_mutex_held() and sqlite3_mutex_notheld() routines
** are intended for use inside assert() statements.  ^the sqlite core
** never uses these routines except inside an assert() and applications
** are advised to follow the lead of the core.  ^the sqlite core only
** provides implementations for these routines when it is compiled
** with the sqlite_debug flag.  ^external mutex implementations
** are only required to provide these routines if sqlite_debug is
** defined and if ndebug is not defined.
**
** ^these routines should return true if the mutex in their argument
** is held or not held, respectively, by the calling thread.
**
** ^the implementation is not required to provide versions of these
** routines that actually work. if the implementation does not provide working
** versions of these routines, it should at least provide stubs that always
** return true so that one does not get spurious assertion failures.
**
** ^if the argument to sqlite3_mutex_held() is a null pointer then
** the routine should return 1.   this seems counter-intuitive since
** clearly the mutex cannot be held if it does not exist.  but
** the reason the mutex does not exist is because the build is not
** using mutexes.  and we do not want the assert() containing the
** call to sqlite3_mutex_held() to fail, so a non-zero return is
** the appropriate thing to do.  ^the sqlite3_mutex_notheld()
** interface should also return 1 when given a null pointer.
*/
#ifndef ndebug
sqlite_api int sqlite3_mutex_held(sqlite3_mutex*);
sqlite_api int sqlite3_mutex_notheld(sqlite3_mutex*);
#endif

/*
** capi3ref: mutex types
**
** the [sqlite3_mutex_alloc()] interface takes a single argument
** which is one of these integer constants.
**
** the set of static mutexes may change from one sqlite release to the
** next.  applications that override the built-in mutex logic must be
** prepared to accommodate additional static mutexes.
*/
#define sqlite_mutex_fast             0
#define sqlite_mutex_recursive        1
#define sqlite_mutex_static_master    2
#define sqlite_mutex_static_mem       3  /* sqlite3_malloc() */
#define sqlite_mutex_static_mem2      4  /* not used */
#define sqlite_mutex_static_open      4  /* sqlite3btreeopen() */
#define sqlite_mutex_static_prng      5  /* sqlite3_random() */
#define sqlite_mutex_static_lru       6  /* lru page list */
#define sqlite_mutex_static_lru2      7  /* not used */
#define sqlite_mutex_static_pmem      7  /* sqlite3pagemalloc() */
#define sqlite_mutex_static_app1      8  /* for use by application */
#define sqlite_mutex_static_app2      9  /* for use by application */
#define sqlite_mutex_static_app3     10  /* for use by application */

/*
** capi3ref: retrieve the mutex for a database connection
**
** ^this interface returns a pointer the [sqlite3_mutex] object that 
** serializes access to the [database connection] given in the argument
** when the [threading mode] is serialized.
** ^if the [threading mode] is single-thread or multi-thread then this
** routine returns a null pointer.
*/
sqlite_api sqlite3_mutex *sqlite3_db_mutex(sqlite3*);

/*
** capi3ref: low-level control of database files
**
** ^the [sqlite3_file_control()] interface makes a direct call to the
** xfilecontrol method for the [sqlite3_io_methods] object associated
** with a particular database identified by the second argument. ^the
** name of the database is "main" for the main database or "temp" for the
** temp database, or the name that appears after the as keyword for
** databases that are added using the [attach] sql command.
** ^a null pointer can be used in place of "main" to refer to the
** main database file.
** ^the third and fourth parameters to this routine
** are passed directly through to the second and third parameters of
** the xfilecontrol method.  ^the return value of the xfilecontrol
** method becomes the return value of this routine.
**
** ^the sqlite_fcntl_file_pointer value for the op parameter causes
** a pointer to the underlying [sqlite3_file] object to be written into
** the space pointed to by the 4th parameter.  ^the sqlite_fcntl_file_pointer
** case is a short-circuit path which does not actually invoke the
** underlying sqlite3_io_methods.xfilecontrol method.
**
** ^if the second parameter (zdbname) does not match the name of any
** open database file, then sqlite_error is returned.  ^this error
** code is not remembered and will not be recalled by [sqlite3_errcode()]
** or [sqlite3_errmsg()].  the underlying xfilecontrol method might
** also return sqlite_error.  there is no way to distinguish between
** an incorrect zdbname and an sqlite_error return from the underlying
** xfilecontrol method.
**
** see also: [sqlite_fcntl_lockstate]
*/
sqlite_api int sqlite3_file_control(sqlite3*, const char *zdbname, int op, void*);

/*
** capi3ref: testing interface
**
** ^the sqlite3_test_control() interface is used to read out internal
** state of sqlite and to inject faults into sqlite for testing
** purposes.  ^the first parameter is an operation code that determines
** the number, meaning, and operation of all subsequent parameters.
**
** this interface is not for use by applications.  it exists solely
** for verifying the correct operation of the sqlite library.  depending
** on how the sqlite library is compiled, this interface might not exist.
**
** the details of the operation codes, their meanings, the parameters
** they take, and what they do are all subject to change without notice.
** unlike most of the sqlite api, this function is not guaranteed to
** operate consistently from one release to the next.
*/
sqlite_api int sqlite3_test_control(int op, ...);

/*
** capi3ref: testing interface operation codes
**
** these constants are the valid operation code parameters used
** as the first argument to [sqlite3_test_control()].
**
** these parameters and their meanings are subject to change
** without notice.  these values are for testing purposes only.
** applications should not use any of these parameters or the
** [sqlite3_test_control()] interface.
*/
#define sqlite_testctrl_first                    5
#define sqlite_testctrl_prng_save                5
#define sqlite_testctrl_prng_restore             6
#define sqlite_testctrl_prng_reset               7
#define sqlite_testctrl_bitvec_test              8
#define sqlite_testctrl_fault_install            9
#define sqlite_testctrl_benign_malloc_hooks     10
#define sqlite_testctrl_pending_byte            11
#define sqlite_testctrl_assert                  12
#define sqlite_testctrl_always                  13
#define sqlite_testctrl_reserve                 14
#define sqlite_testctrl_optimizations           15
#define sqlite_testctrl_iskeyword               16
#define sqlite_testctrl_scratchmalloc           17
#define sqlite_testctrl_localtime_fault         18
#define sqlite_testctrl_explain_stmt            19  /* not used */
#define sqlite_testctrl_never_corrupt           20
#define sqlite_testctrl_vdbe_coverage           21
#define sqlite_testctrl_byteorder               22
#define sqlite_testctrl_isinit                  23
#define sqlite_testctrl_sorter_mmap             24
#define sqlite_testctrl_last                    24

/*
** capi3ref: sqlite runtime status
**
** ^this interface is used to retrieve runtime status information
** about the performance of sqlite, and optionally to reset various
** highwater marks.  ^the first argument is an integer code for
** the specific parameter to measure.  ^(recognized integer codes
** are of the form [status parameters | sqlite_status_...].)^
** ^the current value of the parameter is returned into *pcurrent.
** ^the highest recorded value is returned in *phighwater.  ^if the
** resetflag is true, then the highest record value is reset after
** *phighwater is written.  ^(some parameters do not record the highest
** value.  for those parameters
** nothing is written into *phighwater and the resetflag is ignored.)^
** ^(other parameters record only the highwater mark and not the current
** value.  for these latter parameters nothing is written into *pcurrent.)^
**
** ^the sqlite3_status() routine returns sqlite_ok on success and a
** non-zero [error code] on failure.
**
** this routine is threadsafe but is not atomic.  this routine can be
** called while other threads are running the same or different sqlite
** interfaces.  however the values returned in *pcurrent and
** *phighwater reflect the status of sqlite at different points in time
** and it is possible that another thread might change the parameter
** in between the times when *pcurrent and *phighwater are written.
**
** see also: [sqlite3_db_status()]
*/
sqlite_api int sqlite3_status(int op, int *pcurrent, int *phighwater, int resetflag);


/*
** capi3ref: status parameters
** keywords: {status parameters}
**
** these integer constants designate various run-time status parameters
** that can be returned by [sqlite3_status()].
**
** <dl>
** [[sqlite_status_memory_used]] ^(<dt>sqlite_status_memory_used</dt>
** <dd>this parameter is the current amount of memory checked out
** using [sqlite3_malloc()], either directly or indirectly.  the
** figure includes calls made to [sqlite3_malloc()] by the application
** and internal memory usage by the sqlite library.  scratch memory
** controlled by [sqlite_config_scratch] and auxiliary page-cache
** memory controlled by [sqlite_config_pagecache] is not included in
** this parameter.  the amount returned is the sum of the allocation
** sizes as reported by the xsize method in [sqlite3_mem_methods].</dd>)^
**
** [[sqlite_status_malloc_size]] ^(<dt>sqlite_status_malloc_size</dt>
** <dd>this parameter records the largest memory allocation request
** handed to [sqlite3_malloc()] or [sqlite3_realloc()] (or their
** internal equivalents).  only the value returned in the
** *phighwater parameter to [sqlite3_status()] is of interest.  
** the value written into the *pcurrent parameter is undefined.</dd>)^
**
** [[sqlite_status_malloc_count]] ^(<dt>sqlite_status_malloc_count</dt>
** <dd>this parameter records the number of separate memory allocations
** currently checked out.</dd>)^
**
** [[sqlite_status_pagecache_used]] ^(<dt>sqlite_status_pagecache_used</dt>
** <dd>this parameter returns the number of pages used out of the
** [pagecache memory allocator] that was configured using 
** [sqlite_config_pagecache].  the
** value returned is in pages, not in bytes.</dd>)^
**
** [[sqlite_status_pagecache_overflow]] 
** ^(<dt>sqlite_status_pagecache_overflow</dt>
** <dd>this parameter returns the number of bytes of page cache
** allocation which could not be satisfied by the [sqlite_config_pagecache]
** buffer and where forced to overflow to [sqlite3_malloc()].  the
** returned value includes allocations that overflowed because they
** where too large (they were larger than the "sz" parameter to
** [sqlite_config_pagecache]) and allocations that overflowed because
** no space was left in the page cache.</dd>)^
**
** [[sqlite_status_pagecache_size]] ^(<dt>sqlite_status_pagecache_size</dt>
** <dd>this parameter records the largest memory allocation request
** handed to [pagecache memory allocator].  only the value returned in the
** *phighwater parameter to [sqlite3_status()] is of interest.  
** the value written into the *pcurrent parameter is undefined.</dd>)^
**
** [[sqlite_status_scratch_used]] ^(<dt>sqlite_status_scratch_used</dt>
** <dd>this parameter returns the number of allocations used out of the
** [scratch memory allocator] configured using
** [sqlite_config_scratch].  the value returned is in allocations, not
** in bytes.  since a single thread may only have one scratch allocation
** outstanding at time, this parameter also reports the number of threads
** using scratch memory at the same time.</dd>)^
**
** [[sqlite_status_scratch_overflow]] ^(<dt>sqlite_status_scratch_overflow</dt>
** <dd>this parameter returns the number of bytes of scratch memory
** allocation which could not be satisfied by the [sqlite_config_scratch]
** buffer and where forced to overflow to [sqlite3_malloc()].  the values
** returned include overflows because the requested allocation was too
** larger (that is, because the requested allocation was larger than the
** "sz" parameter to [sqlite_config_scratch]) and because no scratch buffer
** slots were available.
** </dd>)^
**
** [[sqlite_status_scratch_size]] ^(<dt>sqlite_status_scratch_size</dt>
** <dd>this parameter records the largest memory allocation request
** handed to [scratch memory allocator].  only the value returned in the
** *phighwater parameter to [sqlite3_status()] is of interest.  
** the value written into the *pcurrent parameter is undefined.</dd>)^
**
** [[sqlite_status_parser_stack]] ^(<dt>sqlite_status_parser_stack</dt>
** <dd>this parameter records the deepest parser stack.  it is only
** meaningful if sqlite is compiled with [yytrackmaxstackdepth].</dd>)^
** </dl>
**
** new status parameters may be added from time to time.
*/
#define sqlite_status_memory_used          0
#define sqlite_status_pagecache_used       1
#define sqlite_status_pagecache_overflow   2
#define sqlite_status_scratch_used         3
#define sqlite_status_scratch_overflow     4
#define sqlite_status_malloc_size          5
#define sqlite_status_parser_stack         6
#define sqlite_status_pagecache_size       7
#define sqlite_status_scratch_size         8
#define sqlite_status_malloc_count         9

/*
** capi3ref: database connection status
**
** ^this interface is used to retrieve runtime status information 
** about a single [database connection].  ^the first argument is the
** database connection object to be interrogated.  ^the second argument
** is an integer constant, taken from the set of
** [sqlite_dbstatus options], that
** determines the parameter to interrogate.  the set of 
** [sqlite_dbstatus options] is likely
** to grow in future releases of sqlite.
**
** ^the current value of the requested parameter is written into *pcur
** and the highest instantaneous value is written into *phiwtr.  ^if
** the resetflg is true, then the highest instantaneous value is
** reset back down to the current value.
**
** ^the sqlite3_db_status() routine returns sqlite_ok on success and a
** non-zero [error code] on failure.
**
** see also: [sqlite3_status()] and [sqlite3_stmt_status()].
*/
sqlite_api int sqlite3_db_status(sqlite3*, int op, int *pcur, int *phiwtr, int resetflg);

/*
** capi3ref: status parameters for database connections
** keywords: {sqlite_dbstatus options}
**
** these constants are the available integer "verbs" that can be passed as
** the second argument to the [sqlite3_db_status()] interface.
**
** new verbs may be added in future releases of sqlite. existing verbs
** might be discontinued. applications should check the return code from
** [sqlite3_db_status()] to make sure that the call worked.
** the [sqlite3_db_status()] interface will return a non-zero error code
** if a discontinued or unsupported verb is invoked.
**
** <dl>
** [[sqlite_dbstatus_lookaside_used]] ^(<dt>sqlite_dbstatus_lookaside_used</dt>
** <dd>this parameter returns the number of lookaside memory slots currently
** checked out.</dd>)^
**
** [[sqlite_dbstatus_lookaside_hit]] ^(<dt>sqlite_dbstatus_lookaside_hit</dt>
** <dd>this parameter returns the number malloc attempts that were 
** satisfied using lookaside memory. only the high-water value is meaningful;
** the current value is always zero.)^
**
** [[sqlite_dbstatus_lookaside_miss_size]]
** ^(<dt>sqlite_dbstatus_lookaside_miss_size</dt>
** <dd>this parameter returns the number malloc attempts that might have
** been satisfied using lookaside memory but failed due to the amount of
** memory requested being larger than the lookaside slot size.
** only the high-water value is meaningful;
** the current value is always zero.)^
**
** [[sqlite_dbstatus_lookaside_miss_full]]
** ^(<dt>sqlite_dbstatus_lookaside_miss_full</dt>
** <dd>this parameter returns the number malloc attempts that might have
** been satisfied using lookaside memory but failed due to all lookaside
** memory already being in use.
** only the high-water value is meaningful;
** the current value is always zero.)^
**
** [[sqlite_dbstatus_cache_used]] ^(<dt>sqlite_dbstatus_cache_used</dt>
** <dd>this parameter returns the approximate number of bytes of heap
** memory used by all pager caches associated with the database connection.)^
** ^the highwater mark associated with sqlite_dbstatus_cache_used is always 0.
**
** [[sqlite_dbstatus_schema_used]] ^(<dt>sqlite_dbstatus_schema_used</dt>
** <dd>this parameter returns the approximate number of bytes of heap
** memory used to store the schema for all databases associated
** with the connection - main, temp, and any [attach]-ed databases.)^ 
** ^the full amount of memory used by the schemas is reported, even if the
** schema memory is shared with other database connections due to
** [shared cache mode] being enabled.
** ^the highwater mark associated with sqlite_dbstatus_schema_used is always 0.
**
** [[sqlite_dbstatus_stmt_used]] ^(<dt>sqlite_dbstatus_stmt_used</dt>
** <dd>this parameter returns the approximate number of bytes of heap
** and lookaside memory used by all prepared statements associated with
** the database connection.)^
** ^the highwater mark associated with sqlite_dbstatus_stmt_used is always 0.
** </dd>
**
** [[sqlite_dbstatus_cache_hit]] ^(<dt>sqlite_dbstatus_cache_hit</dt>
** <dd>this parameter returns the number of pager cache hits that have
** occurred.)^ ^the highwater mark associated with sqlite_dbstatus_cache_hit 
** is always 0.
** </dd>
**
** [[sqlite_dbstatus_cache_miss]] ^(<dt>sqlite_dbstatus_cache_miss</dt>
** <dd>this parameter returns the number of pager cache misses that have
** occurred.)^ ^the highwater mark associated with sqlite_dbstatus_cache_miss 
** is always 0.
** </dd>
**
** [[sqlite_dbstatus_cache_write]] ^(<dt>sqlite_dbstatus_cache_write</dt>
** <dd>this parameter returns the number of dirty cache entries that have
** been written to disk. specifically, the number of pages written to the
** wal file in wal mode databases, or the number of pages written to the
** database file in rollback mode databases. any pages written as part of
** transaction rollback or database recovery operations are not included.
** if an io or other error occurs while writing a page to disk, the effect
** on subsequent sqlite_dbstatus_cache_write requests is undefined.)^ ^the
** highwater mark associated with sqlite_dbstatus_cache_write is always 0.
** </dd>
**
** [[sqlite_dbstatus_deferred_fks]] ^(<dt>sqlite_dbstatus_deferred_fks</dt>
** <dd>this parameter returns zero for the current value if and only if
** all foreign key constraints (deferred or immediate) have been
** resolved.)^  ^the highwater mark is always 0.
** </dd>
** </dl>
*/
#define sqlite_dbstatus_lookaside_used       0
#define sqlite_dbstatus_cache_used           1
#define sqlite_dbstatus_schema_used          2
#define sqlite_dbstatus_stmt_used            3
#define sqlite_dbstatus_lookaside_hit        4
#define sqlite_dbstatus_lookaside_miss_size  5
#define sqlite_dbstatus_lookaside_miss_full  6
#define sqlite_dbstatus_cache_hit            7
#define sqlite_dbstatus_cache_miss           8
#define sqlite_dbstatus_cache_write          9
#define sqlite_dbstatus_deferred_fks        10
#define sqlite_dbstatus_max                 10   /* largest defined dbstatus */


/*
** capi3ref: prepared statement status
**
** ^(each prepared statement maintains various
** [sqlite_stmtstatus counters] that measure the number
** of times it has performed specific operations.)^  these counters can
** be used to monitor the performance characteristics of the prepared
** statements.  for example, if the number of table steps greatly exceeds
** the number of table searches or result rows, that would tend to indicate
** that the prepared statement is using a full table scan rather than
** an index.  
**
** ^(this interface is used to retrieve and reset counter values from
** a [prepared statement].  the first argument is the prepared statement
** object to be interrogated.  the second argument
** is an integer code for a specific [sqlite_stmtstatus counter]
** to be interrogated.)^
** ^the current value of the requested counter is returned.
** ^if the resetflg is true, then the counter is reset to zero after this
** interface call returns.
**
** see also: [sqlite3_status()] and [sqlite3_db_status()].
*/
sqlite_api int sqlite3_stmt_status(sqlite3_stmt*, int op,int resetflg);

/*
** capi3ref: status parameters for prepared statements
** keywords: {sqlite_stmtstatus counter} {sqlite_stmtstatus counters}
**
** these preprocessor macros define integer codes that name counter
** values associated with the [sqlite3_stmt_status()] interface.
** the meanings of the various counters are as follows:
**
** <dl>
** [[sqlite_stmtstatus_fullscan_step]] <dt>sqlite_stmtstatus_fullscan_step</dt>
** <dd>^this is the number of times that sqlite has stepped forward in
** a table as part of a full table scan.  large numbers for this counter
** may indicate opportunities for performance improvement through 
** careful use of indices.</dd>
**
** [[sqlite_stmtstatus_sort]] <dt>sqlite_stmtstatus_sort</dt>
** <dd>^this is the number of sort operations that have occurred.
** a non-zero value in this counter may indicate an opportunity to
** improvement performance through careful use of indices.</dd>
**
** [[sqlite_stmtstatus_autoindex]] <dt>sqlite_stmtstatus_autoindex</dt>
** <dd>^this is the number of rows inserted into transient indices that
** were created automatically in order to help joins run faster.
** a non-zero value in this counter may indicate an opportunity to
** improvement performance by adding permanent indices that do not
** need to be reinitialized each time the statement is run.</dd>
**
** [[sqlite_stmtstatus_vm_step]] <dt>sqlite_stmtstatus_vm_step</dt>
** <dd>^this is the number of virtual machine operations executed
** by the prepared statement if that number is less than or equal
** to 2147483647.  the number of virtual machine operations can be 
** used as a proxy for the total work done by the prepared statement.
** if the number of virtual machine operations exceeds 2147483647
** then the value returned by this statement status code is undefined.
** </dd>
** </dl>
*/
#define sqlite_stmtstatus_fullscan_step     1
#define sqlite_stmtstatus_sort              2
#define sqlite_stmtstatus_autoindex         3
#define sqlite_stmtstatus_vm_step           4

/*
** capi3ref: custom page cache object
**
** the sqlite3_pcache type is opaque.  it is implemented by
** the pluggable module.  the sqlite core has no knowledge of
** its size or internal structure and never deals with the
** sqlite3_pcache object except by holding and passing pointers
** to the object.
**
** see [sqlite3_pcache_methods2] for additional information.
*/
typedef struct sqlite3_pcache sqlite3_pcache;

/*
** capi3ref: custom page cache object
**
** the sqlite3_pcache_page object represents a single page in the
** page cache.  the page cache will allocate instances of this
** object.  various methods of the page cache use pointers to instances
** of this object as parameters or as their return value.
**
** see [sqlite3_pcache_methods2] for additional information.
*/
typedef struct sqlite3_pcache_page sqlite3_pcache_page;
struct sqlite3_pcache_page {
  void *pbuf;        /* the content of the page */
  void *pextra;      /* extra information associated with the page */
};

/*
** capi3ref: application defined page cache.
** keywords: {page cache}
**
** ^(the [sqlite3_config]([sqlite_config_pcache2], ...) interface can
** register an alternative page cache implementation by passing in an 
** instance of the sqlite3_pcache_methods2 structure.)^
** in many applications, most of the heap memory allocated by 
** sqlite is used for the page cache.
** by implementing a 
** custom page cache using this api, an application can better control
** the amount of memory consumed by sqlite, the way in which 
** that memory is allocated and released, and the policies used to 
** determine exactly which parts of a database file are cached and for 
** how long.
**
** the alternative page cache mechanism is an
** extreme measure that is only needed by the most demanding applications.
** the built-in page cache is recommended for most uses.
**
** ^(the contents of the sqlite3_pcache_methods2 structure are copied to an
** internal buffer by sqlite within the call to [sqlite3_config].  hence
** the application may discard the parameter after the call to
** [sqlite3_config()] returns.)^
**
** [[the xinit() page cache method]]
** ^(the xinit() method is called once for each effective 
** call to [sqlite3_initialize()])^
** (usually only once during the lifetime of the process). ^(the xinit()
** method is passed a copy of the sqlite3_pcache_methods2.parg value.)^
** the intent of the xinit() method is to set up global data structures 
** required by the custom page cache implementation. 
** ^(if the xinit() method is null, then the 
** built-in default page cache is used instead of the application defined
** page cache.)^
**
** [[the xshutdown() page cache method]]
** ^the xshutdown() method is called by [sqlite3_shutdown()].
** it can be used to clean up 
** any outstanding resources before process shutdown, if required.
** ^the xshutdown() method may be null.
**
** ^sqlite automatically serializes calls to the xinit method,
** so the xinit method need not be threadsafe.  ^the
** xshutdown method is only called from [sqlite3_shutdown()] so it does
** not need to be threadsafe either.  all other methods must be threadsafe
** in multithreaded applications.
**
** ^sqlite will never invoke xinit() more than once without an intervening
** call to xshutdown().
**
** [[the xcreate() page cache methods]]
** ^sqlite invokes the xcreate() method to construct a new cache instance.
** sqlite will typically create one cache instance for each open database file,
** though this is not guaranteed. ^the
** first parameter, szpage, is the size in bytes of the pages that must
** be allocated by the cache.  ^szpage will always a power of two.  ^the
** second parameter szextra is a number of bytes of extra storage 
** associated with each page cache entry.  ^the szextra parameter will
** a number less than 250.  sqlite will use the
** extra szextra bytes on each page to store metadata about the underlying
** database page on disk.  the value passed into szextra depends
** on the sqlite version, the target platform, and how sqlite was compiled.
** ^the third argument to xcreate(), bpurgeable, is true if the cache being
** created will be used to cache database pages of a file stored on disk, or
** false if it is used for an in-memory database. the cache implementation
** does not have to do anything special based with the value of bpurgeable;
** it is purely advisory.  ^on a cache where bpurgeable is false, sqlite will
** never invoke xunpin() except to deliberately delete a page.
** ^in other words, calls to xunpin() on a cache with bpurgeable set to
** false will always have the "discard" flag set to true.  
** ^hence, a cache created with bpurgeable false will
** never contain any unpinned pages.
**
** [[the xcachesize() page cache method]]
** ^(the xcachesize() method may be called at any time by sqlite to set the
** suggested maximum cache-size (number of pages stored by) the cache
** instance passed as the first argument. this is the value configured using
** the sqlite "[pragma cache_size]" command.)^  as with the bpurgeable
** parameter, the implementation is not required to do anything with this
** value; it is advisory only.
**
** [[the xpagecount() page cache methods]]
** the xpagecount() method must return the number of pages currently
** stored in the cache, both pinned and unpinned.
** 
** [[the xfetch() page cache methods]]
** the xfetch() method locates a page in the cache and returns a pointer to 
** an sqlite3_pcache_page object associated with that page, or a null pointer.
** the pbuf element of the returned sqlite3_pcache_page object will be a
** pointer to a buffer of szpage bytes used to store the content of a 
** single database page.  the pextra element of sqlite3_pcache_page will be
** a pointer to the szextra bytes of extra storage that sqlite has requested
** for each entry in the page cache.
**
** the page to be fetched is determined by the key. ^the minimum key value
** is 1.  after it has been retrieved using xfetch, the page is considered
** to be "pinned".
**
** if the requested page is already in the page cache, then the page cache
** implementation must return a pointer to the page buffer with its content
** intact.  if the requested page is not already in the cache, then the
** cache implementation should use the value of the createflag
** parameter to help it determined what action to take:
**
** <table border=1 width=85% align=center>
** <tr><th> createflag <th> behavior when page is not already in cache
** <tr><td> 0 <td> do not allocate a new page.  return null.
** <tr><td> 1 <td> allocate a new page if it easy and convenient to do so.
**                 otherwise return null.
** <tr><td> 2 <td> make every effort to allocate a new page.  only return
**                 null if allocating a new page is effectively impossible.
** </table>
**
** ^(sqlite will normally invoke xfetch() with a createflag of 0 or 1.  sqlite
** will only use a createflag of 2 after a prior call with a createflag of 1
** failed.)^  in between the to xfetch() calls, sqlite may
** attempt to unpin one or more cache pages by spilling the content of
** pinned pages to disk and synching the operating system disk cache.
**
** [[the xunpin() page cache method]]
** ^xunpin() is called by sqlite with a pointer to a currently pinned page
** as its second argument.  if the third parameter, discard, is non-zero,
** then the page must be evicted from the cache.
** ^if the discard parameter is
** zero, then the page may be discarded or retained at the discretion of
** page cache implementation. ^the page cache implementation
** may choose to evict unpinned pages at any time.
**
** the cache must not perform any reference counting. a single 
** call to xunpin() unpins the page regardless of the number of prior calls 
** to xfetch().
**
** [[the xrekey() page cache methods]]
** the xrekey() method is used to change the key value associated with the
** page passed as the second argument. if the cache
** previously contains an entry associated with newkey, it must be
** discarded. ^any prior cache entry associated with newkey is guaranteed not
** to be pinned.
**
** when sqlite calls the xtruncate() method, the cache must discard all
** existing cache entries with page numbers (keys) greater than or equal
** to the value of the ilimit parameter passed to xtruncate(). if any
** of these pages are pinned, they are implicitly unpinned, meaning that
** they can be safely discarded.
**
** [[the xdestroy() page cache method]]
** ^the xdestroy() method is used to delete a cache allocated by xcreate().
** all resources associated with the specified cache should be freed. ^after
** calling the xdestroy() method, sqlite considers the [sqlite3_pcache*]
** handle invalid, and will not use it with any other sqlite3_pcache_methods2
** functions.
**
** [[the xshrink() page cache method]]
** ^sqlite invokes the xshrink() method when it wants the page cache to
** free up as much of heap memory as possible.  the page cache implementation
** is not obligated to free any memory, but well-behaved implementations should
** do their best.
*/
typedef struct sqlite3_pcache_methods2 sqlite3_pcache_methods2;
struct sqlite3_pcache_methods2 {
  int iversion;
  void *parg;
  int (*xinit)(void*);
  void (*xshutdown)(void*);
  sqlite3_pcache *(*xcreate)(int szpage, int szextra, int bpurgeable);
  void (*xcachesize)(sqlite3_pcache*, int ncachesize);
  int (*xpagecount)(sqlite3_pcache*);
  sqlite3_pcache_page *(*xfetch)(sqlite3_pcache*, unsigned key, int createflag);
  void (*xunpin)(sqlite3_pcache*, sqlite3_pcache_page*, int discard);
  void (*xrekey)(sqlite3_pcache*, sqlite3_pcache_page*, 
      unsigned oldkey, unsigned newkey);
  void (*xtruncate)(sqlite3_pcache*, unsigned ilimit);
  void (*xdestroy)(sqlite3_pcache*);
  void (*xshrink)(sqlite3_pcache*);
};

/*
** this is the obsolete pcache_methods object that has now been replaced
** by sqlite3_pcache_methods2.  this object is not used by sqlite.  it is
** retained in the header file for backwards compatibility only.
*/
typedef struct sqlite3_pcache_methods sqlite3_pcache_methods;
struct sqlite3_pcache_methods {
  void *parg;
  int (*xinit)(void*);
  void (*xshutdown)(void*);
  sqlite3_pcache *(*xcreate)(int szpage, int bpurgeable);
  void (*xcachesize)(sqlite3_pcache*, int ncachesize);
  int (*xpagecount)(sqlite3_pcache*);
  void *(*xfetch)(sqlite3_pcache*, unsigned key, int createflag);
  void (*xunpin)(sqlite3_pcache*, void*, int discard);
  void (*xrekey)(sqlite3_pcache*, void*, unsigned oldkey, unsigned newkey);
  void (*xtruncate)(sqlite3_pcache*, unsigned ilimit);
  void (*xdestroy)(sqlite3_pcache*);
};


/*
** capi3ref: online backup object
**
** the sqlite3_backup object records state information about an ongoing
** online backup operation.  ^the sqlite3_backup object is created by
** a call to [sqlite3_backup_init()] and is destroyed by a call to
** [sqlite3_backup_finish()].
**
** see also: [using the sqlite online backup api]
*/
typedef struct sqlite3_backup sqlite3_backup;

/*
** capi3ref: online backup api.
**
** the backup api copies the content of one database into another.
** it is useful either for creating backups of databases or
** for copying in-memory databases to or from persistent files. 
**
** see also: [using the sqlite online backup api]
**
** ^sqlite holds a write transaction open on the destination database file
** for the duration of the backup operation.
** ^the source database is read-locked only while it is being read;
** it is not locked continuously for the entire backup operation.
** ^thus, the backup may be performed on a live source database without
** preventing other database connections from
** reading or writing to the source database while the backup is underway.
** 
** ^(to perform a backup operation: 
**   <ol>
**     <li><b>sqlite3_backup_init()</b> is called once to initialize the
**         backup, 
**     <li><b>sqlite3_backup_step()</b> is called one or more times to transfer 
**         the data between the two databases, and finally
**     <li><b>sqlite3_backup_finish()</b> is called to release all resources 
**         associated with the backup operation. 
**   </ol>)^
** there should be exactly one call to sqlite3_backup_finish() for each
** successful call to sqlite3_backup_init().
**
** [[sqlite3_backup_init()]] <b>sqlite3_backup_init()</b>
**
** ^the d and n arguments to sqlite3_backup_init(d,n,s,m) are the 
** [database connection] associated with the destination database 
** and the database name, respectively.
** ^the database name is "main" for the main database, "temp" for the
** temporary database, or the name specified after the as keyword in
** an [attach] statement for an attached database.
** ^the s and m arguments passed to 
** sqlite3_backup_init(d,n,s,m) identify the [database connection]
** and database name of the source database, respectively.
** ^the source and destination [database connections] (parameters s and d)
** must be different or else sqlite3_backup_init(d,n,s,m) will fail with
** an error.
**
** ^if an error occurs within sqlite3_backup_init(d,n,s,m), then null is
** returned and an error code and error message are stored in the
** destination [database connection] d.
** ^the error code and message for the failed call to sqlite3_backup_init()
** can be retrieved using the [sqlite3_errcode()], [sqlite3_errmsg()], and/or
** [sqlite3_errmsg16()] functions.
** ^a successful call to sqlite3_backup_init() returns a pointer to an
** [sqlite3_backup] object.
** ^the [sqlite3_backup] object may be used with the sqlite3_backup_step() and
** sqlite3_backup_finish() functions to perform the specified backup 
** operation.
**
** [[sqlite3_backup_step()]] <b>sqlite3_backup_step()</b>
**
** ^function sqlite3_backup_step(b,n) will copy up to n pages between 
** the source and destination databases specified by [sqlite3_backup] object b.
** ^if n is negative, all remaining source pages are copied. 
** ^if sqlite3_backup_step(b,n) successfully copies n pages and there
** are still more pages to be copied, then the function returns [sqlite_ok].
** ^if sqlite3_backup_step(b,n) successfully finishes copying all pages
** from source to destination, then it returns [sqlite_done].
** ^if an error occurs while running sqlite3_backup_step(b,n),
** then an [error code] is returned. ^as well as [sqlite_ok] and
** [sqlite_done], a call to sqlite3_backup_step() may return [sqlite_readonly],
** [sqlite_nomem], [sqlite_busy], [sqlite_locked], or an
** [sqlite_ioerr_access | sqlite_ioerr_xxx] extended error code.
**
** ^(the sqlite3_backup_step() might return [sqlite_readonly] if
** <ol>
** <li> the destination database was opened read-only, or
** <li> the destination database is using write-ahead-log journaling
** and the destination and source page sizes differ, or
** <li> the destination database is an in-memory database and the
** destination and source page sizes differ.
** </ol>)^
**
** ^if sqlite3_backup_step() cannot obtain a required file-system lock, then
** the [sqlite3_busy_handler | busy-handler function]
** is invoked (if one is specified). ^if the 
** busy-handler returns non-zero before the lock is available, then 
** [sqlite_busy] is returned to the caller. ^in this case the call to
** sqlite3_backup_step() can be retried later. ^if the source
** [database connection]
** is being used to write to the source database when sqlite3_backup_step()
** is called, then [sqlite_locked] is returned immediately. ^again, in this
** case the call to sqlite3_backup_step() can be retried later on. ^(if
** [sqlite_ioerr_access | sqlite_ioerr_xxx], [sqlite_nomem], or
** [sqlite_readonly] is returned, then 
** there is no point in retrying the call to sqlite3_backup_step(). these 
** errors are considered fatal.)^  the application must accept 
** that the backup operation has failed and pass the backup operation handle 
** to the sqlite3_backup_finish() to release associated resources.
**
** ^the first call to sqlite3_backup_step() obtains an exclusive lock
** on the destination file. ^the exclusive lock is not released until either 
** sqlite3_backup_finish() is called or the backup operation is complete 
** and sqlite3_backup_step() returns [sqlite_done].  ^every call to
** sqlite3_backup_step() obtains a [shared lock] on the source database that
** lasts for the duration of the sqlite3_backup_step() call.
** ^because the source database is not locked between calls to
** sqlite3_backup_step(), the source database may be modified mid-way
** through the backup process.  ^if the source database is modified by an
** external process or via a database connection other than the one being
** used by the backup operation, then the backup will be automatically
** restarted by the next call to sqlite3_backup_step(). ^if the source 
** database is modified by the using the same database connection as is used
** by the backup operation, then the backup database is automatically
** updated at the same time.
**
** [[sqlite3_backup_finish()]] <b>sqlite3_backup_finish()</b>
**
** when sqlite3_backup_step() has returned [sqlite_done], or when the 
** application wishes to abandon the backup operation, the application
** should destroy the [sqlite3_backup] by passing it to sqlite3_backup_finish().
** ^the sqlite3_backup_finish() interfaces releases all
** resources associated with the [sqlite3_backup] object. 
** ^if sqlite3_backup_step() has not yet returned [sqlite_done], then any
** active write-transaction on the destination database is rolled back.
** the [sqlite3_backup] object is invalid
** and may not be used following a call to sqlite3_backup_finish().
**
** ^the value returned by sqlite3_backup_finish is [sqlite_ok] if no
** sqlite3_backup_step() errors occurred, regardless or whether or not
** sqlite3_backup_step() completed.
** ^if an out-of-memory condition or io error occurred during any prior
** sqlite3_backup_step() call on the same [sqlite3_backup] object, then
** sqlite3_backup_finish() returns the corresponding [error code].
**
** ^a return of [sqlite_busy] or [sqlite_locked] from sqlite3_backup_step()
** is not a permanent error and does not affect the return value of
** sqlite3_backup_finish().
**
** [[sqlite3_backup__remaining()]] [[sqlite3_backup_pagecount()]]
** <b>sqlite3_backup_remaining() and sqlite3_backup_pagecount()</b>
**
** ^each call to sqlite3_backup_step() sets two values inside
** the [sqlite3_backup] object: the number of pages still to be backed
** up and the total number of pages in the source database file.
** the sqlite3_backup_remaining() and sqlite3_backup_pagecount() interfaces
** retrieve these two values, respectively.
**
** ^the values returned by these functions are only updated by
** sqlite3_backup_step(). ^if the source database is modified during a backup
** operation, then the values are not updated to account for any extra
** pages that need to be updated or the size of the source database file
** changing.
**
** <b>concurrent usage of database handles</b>
**
** ^the source [database connection] may be used by the application for other
** purposes while a backup operation is underway or being initialized.
** ^if sqlite is compiled and configured to support threadsafe database
** connections, then the source database connection may be used concurrently
** from within other threads.
**
** however, the application must guarantee that the destination 
** [database connection] is not passed to any other api (by any thread) after 
** sqlite3_backup_init() is called and before the corresponding call to
** sqlite3_backup_finish().  sqlite does not currently check to see
** if the application incorrectly accesses the destination [database connection]
** and so no error code is reported, but the operations may malfunction
** nevertheless.  use of the destination database connection while a
** backup is in progress might also also cause a mutex deadlock.
**
** if running in [shared cache mode], the application must
** guarantee that the shared cache used by the destination database
** is not accessed while the backup is running. in practice this means
** that the application must guarantee that the disk file being 
** backed up to is not accessed by any connection within the process,
** not just the specific connection that was passed to sqlite3_backup_init().
**
** the [sqlite3_backup] object itself is partially threadsafe. multiple 
** threads may safely make multiple concurrent calls to sqlite3_backup_step().
** however, the sqlite3_backup_remaining() and sqlite3_backup_pagecount()
** apis are not strictly speaking threadsafe. if they are invoked at the
** same time as another thread is invoking sqlite3_backup_step() it is
** possible that they return invalid values.
*/
sqlite_api sqlite3_backup *sqlite3_backup_init(
  sqlite3 *pdest,                        /* destination database handle */
  const char *zdestname,                 /* destination database name */
  sqlite3 *psource,                      /* source database handle */
  const char *zsourcename                /* source database name */
);
sqlite_api int sqlite3_backup_step(sqlite3_backup *p, int npage);
sqlite_api int sqlite3_backup_finish(sqlite3_backup *p);
sqlite_api int sqlite3_backup_remaining(sqlite3_backup *p);
sqlite_api int sqlite3_backup_pagecount(sqlite3_backup *p);

/*
** capi3ref: unlock notification
**
** ^when running in shared-cache mode, a database operation may fail with
** an [sqlite_locked] error if the required locks on the shared-cache or
** individual tables within the shared-cache cannot be obtained. see
** [sqlite shared-cache mode] for a description of shared-cache locking. 
** ^this api may be used to register a callback that sqlite will invoke 
** when the connection currently holding the required lock relinquishes it.
** ^this api is only available if the library was compiled with the
** [sqlite_enable_unlock_notify] c-preprocessor symbol defined.
**
** see also: [using the sqlite unlock notification feature].
**
** ^shared-cache locks are released when a database connection concludes
** its current transaction, either by committing it or rolling it back. 
**
** ^when a connection (known as the blocked connection) fails to obtain a
** shared-cache lock and sqlite_locked is returned to the caller, the
** identity of the database connection (the blocking connection) that
** has locked the required resource is stored internally. ^after an 
** application receives an sqlite_locked error, it may call the
** sqlite3_unlock_notify() method with the blocked connection handle as 
** the first argument to register for a callback that will be invoked
** when the blocking connections current transaction is concluded. ^the
** callback is invoked from within the [sqlite3_step] or [sqlite3_close]
** call that concludes the blocking connections transaction.
**
** ^(if sqlite3_unlock_notify() is called in a multi-threaded application,
** there is a chance that the blocking connection will have already
** concluded its transaction by the time sqlite3_unlock_notify() is invoked.
** if this happens, then the specified callback is invoked immediately,
** from within the call to sqlite3_unlock_notify().)^
**
** ^if the blocked connection is attempting to obtain a write-lock on a
** shared-cache table, and more than one other connection currently holds
** a read-lock on the same table, then sqlite arbitrarily selects one of 
** the other connections to use as the blocking connection.
**
** ^(there may be at most one unlock-notify callback registered by a 
** blocked connection. if sqlite3_unlock_notify() is called when the
** blocked connection already has a registered unlock-notify callback,
** then the new callback replaces the old.)^ ^if sqlite3_unlock_notify() is
** called with a null pointer as its second argument, then any existing
** unlock-notify callback is canceled. ^the blocked connections 
** unlock-notify callback may also be canceled by closing the blocked
** connection using [sqlite3_close()].
**
** the unlock-notify callback is not reentrant. if an application invokes
** any sqlite3_xxx api functions from within an unlock-notify callback, a
** crash or deadlock may be the result.
**
** ^unless deadlock is detected (see below), sqlite3_unlock_notify() always
** returns sqlite_ok.
**
** <b>callback invocation details</b>
**
** when an unlock-notify callback is registered, the application provides a 
** single void* pointer that is passed to the callback when it is invoked.
** however, the signature of the callback function allows sqlite to pass
** it an array of void* context pointers. the first argument passed to
** an unlock-notify callback is a pointer to an array of void* pointers,
** and the second is the number of entries in the array.
**
** when a blocking connections transaction is concluded, there may be
** more than one blocked connection that has registered for an unlock-notify
** callback. ^if two or more such blocked connections have specified the
** same callback function, then instead of invoking the callback function
** multiple times, it is invoked once with the set of void* context pointers
** specified by the blocked connections bundled together into an array.
** this gives the application an opportunity to prioritize any actions 
** related to the set of unblocked database connections.
**
** <b>deadlock detection</b>
**
** assuming that after registering for an unlock-notify callback a 
** database waits for the callback to be issued before taking any further
** action (a reasonable assumption), then using this api may cause the
** application to deadlock. for example, if connection x is waiting for
** connection y's transaction to be concluded, and similarly connection
** y is waiting on connection x's transaction, then neither connection
** will proceed and the system may remain deadlocked indefinitely.
**
** to avoid this scenario, the sqlite3_unlock_notify() performs deadlock
** detection. ^if a given call to sqlite3_unlock_notify() would put the
** system in a deadlocked state, then sqlite_locked is returned and no
** unlock-notify callback is registered. the system is said to be in
** a deadlocked state if connection a has registered for an unlock-notify
** callback on the conclusion of connection b's transaction, and connection
** b has itself registered for an unlock-notify callback when connection
** a's transaction is concluded. ^indirect deadlock is also detected, so
** the system is also considered to be deadlocked if connection b has
** registered for an unlock-notify callback on the conclusion of connection
** c's transaction, where connection c is waiting on connection a. ^any
** number of levels of indirection are allowed.
**
** <b>the "drop table" exception</b>
**
** when a call to [sqlite3_step()] returns sqlite_locked, it is almost 
** always appropriate to call sqlite3_unlock_notify(). there is however,
** one exception. when executing a "drop table" or "drop index" statement,
** sqlite checks if there are any currently executing select statements
** that belong to the same connection. if there are, sqlite_locked is
** returned. in this case there is no "blocking connection", so invoking
** sqlite3_unlock_notify() results in the unlock-notify callback being
** invoked immediately. if the application then re-attempts the "drop table"
** or "drop index" query, an infinite loop might be the result.
**
** one way around this problem is to check the extended error code returned
** by an sqlite3_step() call. ^(if there is a blocking connection, then the
** extended error code is set to sqlite_locked_sharedcache. otherwise, in
** the special "drop table/index" case, the extended error code is just 
** sqlite_locked.)^
*/
sqlite_api int sqlite3_unlock_notify(
  sqlite3 *pblocked,                          /* waiting connection */
  void (*xnotify)(void **aparg, int narg),    /* callback function to invoke */
  void *pnotifyarg                            /* argument to pass to xnotify */
);


/*
** capi3ref: string comparison
**
** ^the [sqlite3_stricmp()] and [sqlite3_strnicmp()] apis allow applications
** and extensions to compare the contents of two buffers containing utf-8
** strings in a case-independent fashion, using the same definition of "case
** independence" that sqlite uses internally when comparing identifiers.
*/
sqlite_api int sqlite3_stricmp(const char *, const char *);
sqlite_api int sqlite3_strnicmp(const char *, const char *, int);

/*
** capi3ref: string globbing
*
** ^the [sqlite3_strglob(p,x)] interface returns zero if string x matches
** the glob pattern p, and it returns non-zero if string x does not match
** the glob pattern p.  ^the definition of glob pattern matching used in
** [sqlite3_strglob(p,x)] is the same as for the "x glob p" operator in the
** sql dialect used by sqlite.  ^the sqlite3_strglob(p,x) function is case
** sensitive.
**
** note that this routine returns zero on a match and non-zero if the strings
** do not match, the same as [sqlite3_stricmp()] and [sqlite3_strnicmp()].
*/
sqlite_api int sqlite3_strglob(const char *zglob, const char *zstr);

/*
** capi3ref: error logging interface
**
** ^the [sqlite3_log()] interface writes a message into the [error log]
** established by the [sqlite_config_log] option to [sqlite3_config()].
** ^if logging is enabled, the zformat string and subsequent arguments are
** used with [sqlite3_snprintf()] to generate the final output string.
**
** the sqlite3_log() interface is intended for use by extensions such as
** virtual tables, collating functions, and sql functions.  while there is
** nothing to prevent an application from calling sqlite3_log(), doing so
** is considered bad form.
**
** the zformat string must not be null.
**
** to avoid deadlocks and other threading problems, the sqlite3_log() routine
** will not use dynamically allocated memory.  the log message is stored in
** a fixed-length buffer on the stack.  if the log message is longer than
** a few hundred characters, it will be truncated to the length of the
** buffer.
*/
sqlite_api void sqlite3_log(int ierrcode, const char *zformat, ...);

/*
** capi3ref: write-ahead log commit hook
**
** ^the [sqlite3_wal_hook()] function is used to register a callback that
** will be invoked each time a database connection commits data to a
** [write-ahead log] (i.e. whenever a transaction is committed in
** [journal_mode | journal_mode=wal mode]). 
**
** ^the callback is invoked by sqlite after the commit has taken place and 
** the associated write-lock on the database released, so the implementation 
** may read, write or [checkpoint] the database as required.
**
** ^the first parameter passed to the callback function when it is invoked
** is a copy of the third parameter passed to sqlite3_wal_hook() when
** registering the callback. ^the second is a copy of the database handle.
** ^the third parameter is the name of the database that was written to -
** either "main" or the name of an [attach]-ed database. ^the fourth parameter
** is the number of pages currently in the write-ahead log file,
** including those that were just committed.
**
** the callback function should normally return [sqlite_ok].  ^if an error
** code is returned, that error will propagate back up through the
** sqlite code base to cause the statement that provoked the callback
** to report an error, though the commit will have still occurred. if the
** callback returns [sqlite_row] or [sqlite_done], or if it returns a value
** that does not correspond to any valid sqlite error code, the results
** are undefined.
**
** a single database handle may have at most a single write-ahead log callback 
** registered at one time. ^calling [sqlite3_wal_hook()] replaces any
** previously registered write-ahead log callback. ^note that the
** [sqlite3_wal_autocheckpoint()] interface and the
** [wal_autocheckpoint pragma] both invoke [sqlite3_wal_hook()] and will
** those overwrite any prior [sqlite3_wal_hook()] settings.
*/
sqlite_api void *sqlite3_wal_hook(
  sqlite3*, 
  int(*)(void *,sqlite3*,const char*,int),
  void*
);

/*
** capi3ref: configure an auto-checkpoint
**
** ^the [sqlite3_wal_autocheckpoint(d,n)] is a wrapper around
** [sqlite3_wal_hook()] that causes any database on [database connection] d
** to automatically [checkpoint]
** after committing a transaction if there are n or
** more frames in the [write-ahead log] file.  ^passing zero or 
** a negative value as the nframe parameter disables automatic
** checkpoints entirely.
**
** ^the callback registered by this function replaces any existing callback
** registered using [sqlite3_wal_hook()].  ^likewise, registering a callback
** using [sqlite3_wal_hook()] disables the automatic checkpoint mechanism
** configured by this function.
**
** ^the [wal_autocheckpoint pragma] can be used to invoke this interface
** from sql.
**
** ^checkpoints initiated by this mechanism are
** [sqlite3_wal_checkpoint_v2|passive].
**
** ^every new [database connection] defaults to having the auto-checkpoint
** enabled with a threshold of 1000 or [sqlite_default_wal_autocheckpoint]
** pages.  the use of this interface
** is only necessary if the default setting is found to be suboptimal
** for a particular application.
*/
sqlite_api int sqlite3_wal_autocheckpoint(sqlite3 *db, int n);

/*
** capi3ref: checkpoint a database
**
** ^the [sqlite3_wal_checkpoint(d,x)] interface causes database named x
** on [database connection] d to be [checkpointed].  ^if x is null or an
** empty string, then a checkpoint is run on all databases of
** connection d.  ^if the database connection d is not in
** [wal | write-ahead log mode] then this interface is a harmless no-op.
** ^the [sqlite3_wal_checkpoint(d,x)] interface initiates a
** [sqlite3_wal_checkpoint_v2|passive] checkpoint.
** use the [sqlite3_wal_checkpoint_v2()] interface to get a full
** or reset checkpoint.
**
** ^the [wal_checkpoint pragma] can be used to invoke this interface
** from sql.  ^the [sqlite3_wal_autocheckpoint()] interface and the
** [wal_autocheckpoint pragma] can be used to cause this interface to be
** run whenever the wal reaches a certain size threshold.
**
** see also: [sqlite3_wal_checkpoint_v2()]
*/
sqlite_api int sqlite3_wal_checkpoint(sqlite3 *db, const char *zdb);

/*
** capi3ref: checkpoint a database
**
** run a checkpoint operation on wal database zdb attached to database 
** handle db. the specific operation is determined by the value of the 
** emode parameter:
**
** <dl>
** <dt>sqlite_checkpoint_passive<dd>
**   checkpoint as many frames as possible without waiting for any database 
**   readers or writers to finish. sync the db file if all frames in the log
**   are checkpointed. this mode is the same as calling 
**   sqlite3_wal_checkpoint(). the [sqlite3_busy_handler|busy-handler callback]
**   is never invoked.
**
** <dt>sqlite_checkpoint_full<dd>
**   this mode blocks (it invokes the
**   [sqlite3_busy_handler|busy-handler callback]) until there is no
**   database writer and all readers are reading from the most recent database
**   snapshot. it then checkpoints all frames in the log file and syncs the
**   database file. this call blocks database writers while it is running,
**   but not database readers.
**
** <dt>sqlite_checkpoint_restart<dd>
**   this mode works the same way as sqlite_checkpoint_full, except after 
**   checkpointing the log file it blocks (calls the 
**   [sqlite3_busy_handler|busy-handler callback])
**   until all readers are reading from the database file only. this ensures 
**   that the next client to write to the database file restarts the log file 
**   from the beginning. this call blocks database writers while it is running,
**   but not database readers.
** </dl>
**
** if pnlog is not null, then *pnlog is set to the total number of frames in
** the log file before returning. if pnckpt is not null, then *pnckpt is set to
** the total number of checkpointed frames (including any that were already
** checkpointed when this function is called). *pnlog and *pnckpt may be
** populated even if sqlite3_wal_checkpoint_v2() returns other than sqlite_ok.
** if no values are available because of an error, they are both set to -1
** before returning to communicate this to the caller.
**
** all calls obtain an exclusive "checkpoint" lock on the database file. if
** any other process is running a checkpoint operation at the same time, the 
** lock cannot be obtained and sqlite_busy is returned. even if there is a 
** busy-handler configured, it will not be invoked in this case.
**
** the sqlite_checkpoint_full and restart modes also obtain the exclusive 
** "writer" lock on the database file. if the writer lock cannot be obtained
** immediately, and a busy-handler is configured, it is invoked and the writer
** lock retried until either the busy-handler returns 0 or the lock is
** successfully obtained. the busy-handler is also invoked while waiting for
** database readers as described above. if the busy-handler returns 0 before
** the writer lock is obtained or while waiting for database readers, the
** checkpoint operation proceeds from that point in the same way as 
** sqlite_checkpoint_passive - checkpointing as many frames as possible 
** without blocking any further. sqlite_busy is returned in this case.
**
** if parameter zdb is null or points to a zero length string, then the
** specified operation is attempted on all wal databases. in this case the
** values written to output parameters *pnlog and *pnckpt are undefined. if 
** an sqlite_busy error is encountered when processing one or more of the 
** attached wal databases, the operation is still attempted on any remaining 
** attached databases and sqlite_busy is returned to the caller. if any other 
** error occurs while processing an attached database, processing is abandoned 
** and the error code returned to the caller immediately. if no error 
** (sqlite_busy or otherwise) is encountered while processing the attached 
** databases, sqlite_ok is returned.
**
** if database zdb is the name of an attached database that is not in wal
** mode, sqlite_ok is returned and both *pnlog and *pnckpt set to -1. if
** zdb is not null (or a zero length string) and is not the name of any
** attached database, sqlite_error is returned to the caller.
*/
sqlite_api int sqlite3_wal_checkpoint_v2(
  sqlite3 *db,                    /* database handle */
  const char *zdb,                /* name of attached database (or null) */
  int emode,                      /* sqlite_checkpoint_* value */
  int *pnlog,                     /* out: size of wal log in frames */
  int *pnckpt                     /* out: total number of frames checkpointed */
);

/*
** capi3ref: checkpoint operation parameters
**
** these constants can be used as the 3rd parameter to
** [sqlite3_wal_checkpoint_v2()].  see the [sqlite3_wal_checkpoint_v2()]
** documentation for additional information about the meaning and use of
** each of these values.
*/
#define sqlite_checkpoint_passive 0
#define sqlite_checkpoint_full    1
#define sqlite_checkpoint_restart 2

/*
** capi3ref: virtual table interface configuration
**
** this function may be called by either the [xconnect] or [xcreate] method
** of a [virtual table] implementation to configure
** various facets of the virtual table interface.
**
** if this interface is invoked outside the context of an xconnect or
** xcreate virtual table method then the behavior is undefined.
**
** at present, there is only one option that may be configured using
** this function. (see [sqlite_vtab_constraint_support].)  further options
** may be added in the future.
*/
sqlite_api int sqlite3_vtab_config(sqlite3*, int op, ...);

/*
** capi3ref: virtual table configuration options
**
** these macros define the various options to the
** [sqlite3_vtab_config()] interface that [virtual table] implementations
** can use to customize and optimize their behavior.
**
** <dl>
** <dt>sqlite_vtab_constraint_support
** <dd>calls of the form
** [sqlite3_vtab_config](db,sqlite_vtab_constraint_support,x) are supported,
** where x is an integer.  if x is zero, then the [virtual table] whose
** [xcreate] or [xconnect] method invoked [sqlite3_vtab_config()] does not
** support constraints.  in this configuration (which is the default) if
** a call to the [xupdate] method returns [sqlite_constraint], then the entire
** statement is rolled back as if [on conflict | or abort] had been
** specified as part of the users sql statement, regardless of the actual
** on conflict mode specified.
**
** if x is non-zero, then the virtual table implementation guarantees
** that if [xupdate] returns [sqlite_constraint], it will do so before
** any modifications to internal or persistent data structures have been made.
** if the [on conflict] mode is abort, fail, ignore or rollback, sqlite 
** is able to roll back a statement or database transaction, and abandon
** or continue processing the current sql statement as appropriate. 
** if the on conflict mode is replace and the [xupdate] method returns
** [sqlite_constraint], sqlite handles this as if the on conflict mode
** had been abort.
**
** virtual table implementations that are required to handle or replace
** must do so within the [xupdate] method. if a call to the 
** [sqlite3_vtab_on_conflict()] function indicates that the current on 
** conflict policy is replace, the virtual table implementation should 
** silently replace the appropriate rows within the xupdate callback and
** return sqlite_ok. or, if this is not possible, it may return
** sqlite_constraint, in which case sqlite falls back to or abort 
** constraint handling.
** </dl>
*/
#define sqlite_vtab_constraint_support 1

/*
** capi3ref: determine the virtual table conflict policy
**
** this function may only be called from within a call to the [xupdate] method
** of a [virtual table] implementation for an insert or update operation. ^the
** value returned is one of [sqlite_rollback], [sqlite_ignore], [sqlite_fail],
** [sqlite_abort], or [sqlite_replace], according to the [on conflict] mode
** of the sql statement that triggered the call to the [xupdate] method of the
** [virtual table].
*/
sqlite_api int sqlite3_vtab_on_conflict(sqlite3 *);

/*
** capi3ref: conflict resolution modes
** keywords: {conflict resolution mode}
**
** these constants are returned by [sqlite3_vtab_on_conflict()] to
** inform a [virtual table] implementation what the [on conflict] mode
** is for the sql statement being evaluated.
**
** note that the [sqlite_ignore] constant is also used as a potential
** return value from the [sqlite3_set_authorizer()] callback and that
** [sqlite_abort] is also a [result code].
*/
#define sqlite_rollback 1
/* #define sqlite_ignore 2 // also used by sqlite3_authorizer() callback */
#define sqlite_fail     3
/* #define sqlite_abort 4  // also an error code */
#define sqlite_replace  5



/*
** undo the hack that converts floating point types to integer for
** builds on processors without floating point support.
*/
#ifdef sqlite_omit_floating_point
# undef double
#endif

#ifdef __cplusplus
}  /* end of the 'extern "c"' block */
#endif
#endif /* _sqlite3_h_ */

/*
** 2010 august 30
**
** the author disclaims copyright to this source code.  in place of
** a legal notice, here is a blessing:
**
**    may you do good and not evil.
**    may you find forgiveness for yourself and forgive others.
**    may you share freely, never taking more than you give.
**
*************************************************************************
*/

#ifndef _sqlite3rtree_h_
#define _sqlite3rtree_h_


#ifdef __cplusplus
extern "c" {
#endif

typedef struct sqlite3_rtree_geometry sqlite3_rtree_geometry;
typedef struct sqlite3_rtree_query_info sqlite3_rtree_query_info;

/* the double-precision datatype used by rtree depends on the
** sqlite_rtree_int_only compile-time option.
*/
#ifdef sqlite_rtree_int_only
  typedef sqlite3_int64 sqlite3_rtree_dbl;
#else
  typedef double sqlite3_rtree_dbl;
#endif

/*
** register a geometry callback named zgeom that can be used as part of an
** r-tree geometry query as follows:
**
**   select ... from <rtree> where <rtree col> match $zgeom(... params ...)
*/
sqlite_api int sqlite3_rtree_geometry_callback(
  sqlite3 *db,
  const char *zgeom,
  int (*xgeom)(sqlite3_rtree_geometry*, int, sqlite3_rtree_dbl*,int*),
  void *pcontext
);


/*
** a pointer to a structure of the following type is passed as the first
** argument to callbacks registered using rtree_geometry_callback().
*/
struct sqlite3_rtree_geometry {
  void *pcontext;                 /* copy of pcontext passed to s_r_g_c() */
  int nparam;                     /* size of array aparam[] */
  sqlite3_rtree_dbl *aparam;      /* parameters passed to sql geom function */
  void *puser;                    /* callback implementation user data */
  void (*xdeluser)(void *);       /* called by sqlite to clean up puser */
};

/*
** register a 2nd-generation geometry callback named zscore that can be 
** used as part of an r-tree geometry query as follows:
**
**   select ... from <rtree> where <rtree col> match $zqueryfunc(... params ...)
*/
sqlite_api int sqlite3_rtree_query_callback(
  sqlite3 *db,
  const char *zqueryfunc,
  int (*xqueryfunc)(sqlite3_rtree_query_info*),
  void *pcontext,
  void (*xdestructor)(void*)
);


/*
** a pointer to a structure of the following type is passed as the 
** argument to scored geometry callback registered using
** sqlite3_rtree_query_callback().
**
** note that the first 5 fields of this structure are identical to
** sqlite3_rtree_geometry.  this structure is a subclass of
** sqlite3_rtree_geometry.
*/
struct sqlite3_rtree_query_info {
  void *pcontext;                   /* pcontext from when function registered */
  int nparam;                       /* number of function parameters */
  sqlite3_rtree_dbl *aparam;        /* value of function parameters */
  void *puser;                      /* callback can use this, if desired */
  void (*xdeluser)(void*);          /* function to free puser */
  sqlite3_rtree_dbl *acoord;        /* coordinates of node or entry to check */
  unsigned int *anqueue;            /* number of pending entries in the queue */
  int ncoord;                       /* number of coordinates */
  int ilevel;                       /* level of current node or entry */
  int mxlevel;                      /* the largest ilevel value in the tree */
  sqlite3_int64 irowid;             /* rowid for current entry */
  sqlite3_rtree_dbl rparentscore;   /* score of parent node */
  int eparentwithin;                /* visibility of parent node */
  int ewithin;                      /* out: visiblity */
  sqlite3_rtree_dbl rscore;         /* out: write the score here */
};

/*
** allowed values for sqlite3_rtree_query.ewithin and .eparentwithin.
*/
#define not_within       0   /* object completely outside of query region */
#define partly_within    1   /* object partially overlaps query region */
#define fully_within     2   /* object fully contained within query region */


#ifdef __cplusplus
}  /* end of the 'extern "c"' block */
#endif

#endif  /* ifndef _sqlite3rtree_h_ */

