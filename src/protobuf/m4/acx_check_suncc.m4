dnl check for the presence of the sun studio compiler.
dnl if sun studio compiler is found, set appropriate flags.
dnl additionally, sun studio doesn't default to 64-bit by itself,
dnl nor does it automatically look in standard solaris places for
dnl 64-bit libs, so we must add those options and paths to the search
dnl paths.

dnl todo(kenton):  this is pretty hacky.  it sets cxxflags, which the autoconf
dnl docs say should never be overridden except by the user.  it also isn't
dnl cross-compile safe.  we should fix these problems, but since i don't have
dnl sun cc at my disposal for testing, someone else will have to do it.

ac_defun([acx_check_suncc],[

  ac_lang_push([c++])
  ac_check_decl([__sunpro_cc], [suncc="yes"], [suncc="no"])
  ac_lang_pop()


  ac_arg_enable([64bit-solaris],
    [as_help_string([--disable-64bit-solaris],
      [build 64 bit binary on solaris @<:@default=on@:>@])],
             [ac_enable_64bit="$enableval"],
             [ac_enable_64bit="yes"])

  as_if([test "$suncc" = "yes" -a "x${ac_cv_env_cxxflags_set}" = "x"],[
    dnl sun studio has a crashing bug with -xo4 in some cases. keep this
    dnl at -xo3 until a proper test to detect those crashes can be done.
    cxxflags="-g0 -xo3 -xlibmil -xdepend -xbuiltin -mt -compat=5 -library=stlport4 -library=crun -template=no%extdef ${cxxflags}"
  ])

  case $host_os in
    *solaris*)
      ac_check_progs(isainfo, [isainfo], [no])
      as_if([test "x$isainfo" != "xno"],
            [isainfo_b=`${isainfo} -b`],
            [isainfo_b="x"])

      as_if([test "$isainfo_b" != "x"],[

        isainfo_k=`${isainfo} -k`

        as_if([test "x$ac_enable_64bit" = "xyes"],[

          as_if([test "x$libdir" = "x\${exec_prefix}/lib"],[
           dnl the user hasn't overridden the default libdir, so we'll
           dnl the dir suffix to match solaris 32/64-bit policy
           libdir="${libdir}/${isainfo_k}"
          ])

          dnl this should just be set in cppflags and in ldflags, but libtool
          dnl does the wrong thing if you don't put it into cxxflags. sigh.
          dnl (it also needs it in cflags, or it does a different wrong thing!)
          as_if([test "x${ac_cv_env_cxxflags_set}" = "x"],[
            cxxflags="${cxxflags} -m64"
            ac_cv_env_cxxflags_set=set
            ac_cv_env_cxxflags_value='-m64'
          ])

          as_if([test "x${ac_cv_env_cflags_set}" = "x"],[
            cflags="${cflags} -m64"
            ac_cv_env_cflags_set=set
            ac_cv_env_cflags_value='-m64'
          ])

          as_if([test "$target_cpu" = "sparc" -a "x$suncc" = "xyes" ],[
            cxxflags="-xmemalign=8s ${cxxflags}"
          ])
        ])
      ])
    ;;
  esac

])
