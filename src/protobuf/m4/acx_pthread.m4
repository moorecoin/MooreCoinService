# this was retrieved from
#    http://svn.0pointer.de/viewvc/trunk/common/acx_pthread.m4?revision=1277&root=avahi
# see also (perhaps for new versions?)
#    http://svn.0pointer.de/viewvc/trunk/common/acx_pthread.m4?root=avahi
#
# we've rewritten the inconsistency check code (from avahi), to work
# more broadly.  in particular, it no longer assumes ld accepts -zdefs.
# this caused a restructing of the code, but the functionality has only
# changed a little.

dnl @synopsis acx_pthread([action-if-found[, action-if-not-found]])
dnl
dnl @summary figure out how to build c programs using posix threads
dnl
dnl this macro figures out how to build c programs using posix threads.
dnl it sets the pthread_libs output variable to the threads library and
dnl linker flags, and the pthread_cflags output variable to any special
dnl c compiler flags that are needed. (the user can also force certain
dnl compiler flags/libs to be tested by setting these environment
dnl variables.)
dnl
dnl also sets pthread_cc to any special c compiler that is needed for
dnl multi-threaded programs (defaults to the value of cc otherwise).
dnl (this is necessary on aix to use the special cc_r compiler alias.)
dnl
dnl note: you are assumed to not only compile your program with these
dnl flags, but also link it with them as well. e.g. you should link
dnl with $pthread_cc $cflags $pthread_cflags $ldflags ... $pthread_libs
dnl $libs
dnl
dnl if you are only building threads programs, you may wish to use
dnl these variables in your default libs, cflags, and cc:
dnl
dnl        libs="$pthread_libs $libs"
dnl        cflags="$cflags $pthread_cflags"
dnl        cc="$pthread_cc"
dnl
dnl in addition, if the pthread_create_joinable thread-attribute
dnl constant has a nonstandard name, defines pthread_create_joinable to
dnl that name (e.g. pthread_create_undetached on aix).
dnl
dnl action-if-found is a list of shell commands to run if a threads
dnl library is found, and action-if-not-found is a list of commands to
dnl run it if it is not found. if action-if-found is not specified, the
dnl default action will define have_pthread.
dnl
dnl please let the authors know if this macro fails on any platform, or
dnl if you have any other suggestions or comments. this macro was based
dnl on work by sgj on autoconf scripts for fftw (www.fftw.org) (with
dnl help from m. frigo), as well as ac_pthread and hb_pthread macros
dnl posted by alejandro forero cuervo to the autoconf macro repository.
dnl we are also grateful for the helpful feedback of numerous users.
dnl
dnl @category installedpackages
dnl @author steven g. johnson <stevenj@alum.mit.edu>
dnl @version 2006-05-29
dnl @license gplwithacexception
dnl 
dnl checks for gcc shared/pthread inconsistency based on work by
dnl marcin owsiany <marcin@owsiany.pl>


ac_defun([acx_pthread], [
ac_require([ac_canonical_host])
ac_lang_save
ac_lang_c
acx_pthread_ok=no

# we used to check for pthread.h first, but this fails if pthread.h
# requires special compiler flags (e.g. on true64 or sequent).
# it gets checked for in the link test anyway.

# first of all, check if the user has set any of the pthread_libs,
# etcetera environment variables, and if threads linking works using
# them:
if test x"$pthread_libs$pthread_cflags" != x; then
        save_cflags="$cflags"
        cflags="$cflags $pthread_cflags"
        save_libs="$libs"
        libs="$pthread_libs $libs"
        ac_msg_checking([for pthread_join in libs=$pthread_libs with cflags=$pthread_cflags])
        ac_try_link_func(pthread_join, acx_pthread_ok=yes)
        ac_msg_result($acx_pthread_ok)
        if test x"$acx_pthread_ok" = xno; then
                pthread_libs=""
                pthread_cflags=""
        fi
        libs="$save_libs"
        cflags="$save_cflags"
fi

# we must check for the threads library under a number of different
# names; the ordering is very important because some systems
# (e.g. dec) have both -lpthread and -lpthreads, where one of the
# libraries is broken (non-posix).

# create a list of thread flags to try.  items starting with a "-" are
# c compiler flags, and other items are library names, except for "none"
# which indicates that we try without any flags at all, and "pthread-config"
# which is a program returning the flags for the pth emulation library.

acx_pthread_flags="pthreads none -kthread -kthread lthread -pthread -pthreads -mthreads pthread --thread-safe -mt pthread-config"

# the ordering *is* (sometimes) important.  some notes on the
# individual items follow:

# pthreads: aix (must check this before -lpthread)
# none: in case threads are in libc; should be tried before -kthread and
#       other compiler flags to prevent continual compiler warnings
# -kthread: sequent (threads in libc, but -kthread needed for pthread.h)
# -kthread: freebsd kernel threads (preferred to -pthread since smp-able)
# lthread: linuxthreads port on freebsd (also preferred to -pthread)
# -pthread: linux/gcc (kernel threads), bsd/gcc (userland threads)
# -pthreads: solaris/gcc
# -mthreads: mingw32/gcc, lynx/gcc
# -mt: sun workshop c (may only link sunos threads [-lthread], but it
#      doesn't hurt to check since this sometimes defines pthreads too;
#      also defines -d_reentrant)
#      ... -mt is also the pthreads flag for hp/acc
# pthread: linux, etcetera
# --thread-safe: kai c++
# pthread-config: use pthread-config program (for gnu pth library)

case "${host_cpu}-${host_os}" in
        *solaris*)

        # on solaris (at least, for some versions), libc contains stubbed
        # (non-functional) versions of the pthreads routines, so link-based
        # tests will erroneously succeed.  (we need to link with -pthreads/-mt/
        # -lpthread.)  (the stubs are missing pthread_cleanup_push, or rather
        # a function called by this macro, so we could check for that, but
        # who knows whether they'll stub that too in a future libc.)  so,
        # we'll just look for -pthreads and -lpthread first:

        acx_pthread_flags="-pthreads pthread -mt -pthread $acx_pthread_flags"
        ;;
esac

if test x"$acx_pthread_ok" = xno; then
for flag in $acx_pthread_flags; do

        case $flag in
                none)
                ac_msg_checking([whether pthreads work without any flags])
                ;;

                -*)
                ac_msg_checking([whether pthreads work with $flag])
                pthread_cflags="$flag"
                ;;

		pthread-config)
		ac_check_prog(acx_pthread_config, pthread-config, yes, no)
		if test x"$acx_pthread_config" = xno; then continue; fi
		pthread_cflags="`pthread-config --cflags`"
		pthread_libs="`pthread-config --ldflags` `pthread-config --libs`"
		;;

                *)
                ac_msg_checking([for the pthreads library -l$flag])
                pthread_libs="-l$flag"
                ;;
        esac

        save_libs="$libs"
        save_cflags="$cflags"
        libs="$pthread_libs $libs"
        cflags="$cflags $pthread_cflags"

        # check for various functions.  we must include pthread.h,
        # since some functions may be macros.  (on the sequent, we
        # need a special flag -kthread to make this header compile.)
        # we check for pthread_join because it is in -lpthread on irix
        # while pthread_create is in libc.  we check for pthread_attr_init
        # due to dec craziness with -lpthreads.  we check for
        # pthread_cleanup_push because it is one of the few pthread
        # functions on solaris that doesn't have a non-functional libc stub.
        # we try pthread_create on general principles.
        ac_try_link([#include <pthread.h>],
                    [pthread_t th; pthread_join(th, 0);
                     pthread_attr_init(0); pthread_cleanup_push(0, 0);
                     pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
                    [acx_pthread_ok=yes])

        libs="$save_libs"
        cflags="$save_cflags"

        ac_msg_result($acx_pthread_ok)
        if test "x$acx_pthread_ok" = xyes; then
                break;
        fi

        pthread_libs=""
        pthread_cflags=""
done
fi

# various other checks:
if test "x$acx_pthread_ok" = xyes; then
        save_libs="$libs"
        libs="$pthread_libs $libs"
        save_cflags="$cflags"
        cflags="$cflags $pthread_cflags"

        # detect aix lossage: joinable attribute is called undetached.
	ac_msg_checking([for joinable pthread attribute])
	attr_name=unknown
	for attr in pthread_create_joinable pthread_create_undetached; do
	    ac_try_link([#include <pthread.h>], [int attr=$attr; return attr;],
                        [attr_name=$attr; break])
	done
        ac_msg_result($attr_name)
        if test "$attr_name" != pthread_create_joinable; then
            ac_define_unquoted(pthread_create_joinable, $attr_name,
                               [define to necessary symbol if this constant
                                uses a non-standard name on your system.])
        fi

        ac_msg_checking([if more special flags are required for pthreads])
        flag=no
        case "${host_cpu}-${host_os}" in
            *-aix* | *-freebsd* | *-darwin*) flag="-d_thread_safe";;
            *solaris* | *-osf* | *-hpux*) flag="-d_reentrant";;
        esac
        ac_msg_result(${flag})
        if test "x$flag" != xno; then
            pthread_cflags="$flag $pthread_cflags"
        fi

        libs="$save_libs"
        cflags="$save_cflags"
        # more aix lossage: must compile with xlc_r or cc_r
	if test x"$gcc" != xyes; then
          ac_check_progs(pthread_cc, xlc_r cc_r, ${cc})
        else
          pthread_cc=$cc
	fi

	# the next part tries to detect gcc inconsistency with -shared on some
	# architectures and systems. the problem is that in certain
	# configurations, when -shared is specified, gcc "forgets" to
	# internally use various flags which are still necessary.
	
	#
	# prepare the flags
	#
	save_cflags="$cflags"
	save_libs="$libs"
	save_cc="$cc"
	
	# try with the flags determined by the earlier checks.
	#
	# -wl,-z,defs forces link-time symbol resolution, so that the
	# linking checks with -shared actually have any value
	#
	# fixme: -fpic is required for -shared on many architectures,
	# so we specify it here, but the right way would probably be to
	# properly detect whether it is actually required.
	cflags="-shared -fpic -wl,-z,defs $cflags $pthread_cflags"
	libs="$pthread_libs $libs"
	cc="$pthread_cc"
	
	# in order not to create several levels of indentation, we test
	# the value of "$done" until we find the cure or run out of ideas.
	done="no"
	
	# first, make sure the cflags we added are actually accepted by our
	# compiler.  if not (and os x's ld, for instance, does not accept -z),
	# then we can't do this test.
	if test x"$done" = xno; then
	   ac_msg_checking([whether to check for gcc pthread/shared inconsistencies])
	   ac_try_link(,, , [done=yes])
	
	   if test "x$done" = xyes ; then
	      ac_msg_result([no])
	   else
	      ac_msg_result([yes])
	   fi
	fi
	
	if test x"$done" = xno; then
	   ac_msg_checking([whether -pthread is sufficient with -shared])
	   ac_try_link([#include <pthread.h>],
	      [pthread_t th; pthread_join(th, 0);
	      pthread_attr_init(0); pthread_cleanup_push(0, 0);
	      pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
	      [done=yes])
	   
	   if test "x$done" = xyes; then
	      ac_msg_result([yes])
	   else
	      ac_msg_result([no])
	   fi
	fi
	
	#
	# linux gcc on some architectures such as mips/mipsel forgets
	# about -lpthread
	#
	if test x"$done" = xno; then
	   ac_msg_checking([whether -lpthread fixes that])
	   libs="-lpthread $pthread_libs $save_libs"
	   ac_try_link([#include <pthread.h>],
	      [pthread_t th; pthread_join(th, 0);
	      pthread_attr_init(0); pthread_cleanup_push(0, 0);
	      pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
	      [done=yes])
	
	   if test "x$done" = xyes; then
	      ac_msg_result([yes])
	      pthread_libs="-lpthread $pthread_libs"
	   else
	      ac_msg_result([no])
	   fi
	fi
	#
	# freebsd 4.10 gcc forgets to use -lc_r instead of -lc
	#
	if test x"$done" = xno; then
	   ac_msg_checking([whether -lc_r fixes that])
	   libs="-lc_r $pthread_libs $save_libs"
	   ac_try_link([#include <pthread.h>],
	       [pthread_t th; pthread_join(th, 0);
	        pthread_attr_init(0); pthread_cleanup_push(0, 0);
	        pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
	       [done=yes])
	
	   if test "x$done" = xyes; then
	      ac_msg_result([yes])
	      pthread_libs="-lc_r $pthread_libs"
	   else
	      ac_msg_result([no])
	   fi
	fi
	if test x"$done" = xno; then
	   # ok, we have run out of ideas
	   ac_msg_warn([impossible to determine how to use pthreads with shared libraries])
	
	   # so it's not safe to assume that we may use pthreads
	   acx_pthread_ok=no
	fi
	
	ac_msg_checking([whether what we have so far is sufficient with -nostdlib])
	cflags="-nostdlib $cflags"
	# we need c with nostdlib
	libs="$libs -lc"
	ac_try_link([#include <pthread.h>],
	      [pthread_t th; pthread_join(th, 0);
	       pthread_attr_init(0); pthread_cleanup_push(0, 0);
	       pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
	      [done=yes],[done=no])

	if test "x$done" = xyes; then
	   ac_msg_result([yes])
	else
	   ac_msg_result([no])
	fi
	
	if test x"$done" = xno; then
	   ac_msg_checking([whether -lpthread saves the day])
	   libs="-lpthread $libs"
	   ac_try_link([#include <pthread.h>],
	      [pthread_t th; pthread_join(th, 0);
	       pthread_attr_init(0); pthread_cleanup_push(0, 0);
	       pthread_create(0,0,0,0); pthread_cleanup_pop(0); ],
	      [done=yes],[done=no])

	   if test "x$done" = xyes; then
	      ac_msg_result([yes])
	      pthread_libs="$pthread_libs -lpthread"
	   else
	      ac_msg_result([no])
	      ac_msg_warn([impossible to determine how to use pthreads with shared libraries and -nostdlib])
	   fi
	fi

	cflags="$save_cflags"
	libs="$save_libs"
	cc="$save_cc"
else
        pthread_cc="$cc"
fi

ac_subst(pthread_libs)
ac_subst(pthread_cflags)
ac_subst(pthread_cc)

# finally, execute action-if-found/action-if-not-found:
if test x"$acx_pthread_ok" = xyes; then
        ifelse([$1],,ac_define(have_pthread,1,[define if you have posix threads libraries and header files.]),[$1])
        :
else
        acx_pthread_ok=no
        $2
fi
ac_lang_restore
])dnl acx_pthread
