dnl provide ac_use_system_extensions for old autoconf machines.
ac_defun([acx_use_system_extensions],[
  ifdef([ac_use_system_extensions],[
    ac_use_system_extensions
  ],[
    ac_before([$0], [ac_compile_ifelse])
    ac_before([$0], [ac_run_ifelse])

    ac_require([ac_gnu_source])
    ac_require([ac_aix])
    ac_require([ac_minix])

    ah_verbatim([__extensions__],
[/* enable extensions on solaris.  */
#ifndef __extensions__
# undef __extensions__
#endif
#ifndef _posix_pthread_semantics
# undef _posix_pthread_semantics
#endif
#ifndef _tandem_source
# undef _tandem_source
#endif])
    ac_cache_check([whether it is safe to define __extensions__],
      [ac_cv_safe_to_define___extensions__],
      [ac_compile_ifelse(
         [ac_lang_program([
#           define __extensions__ 1
            ac_includes_default])],
         [ac_cv_safe_to_define___extensions__=yes],
         [ac_cv_safe_to_define___extensions__=no])])
    test $ac_cv_safe_to_define___extensions__ = yes &&
      ac_define([__extensions__])
    ac_define([_posix_pthread_semantics])
    ac_define([_tandem_source])
  ])
])
