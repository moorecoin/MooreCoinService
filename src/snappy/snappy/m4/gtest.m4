dnl gtest_lib_check([minimum version [,
dnl                  action if found [,action if not found]]])
dnl
dnl check for the presence of the google test library, optionally at a minimum
dnl version, and indicate a viable version with the have_gtest flag. it defines
dnl standard variables for substitution including gtest_cppflags,
dnl gtest_cxxflags, gtest_ldflags, and gtest_libs. it also defines
dnl gtest_version as the version of google test found. finally, it provides
dnl optional custom action slots in the event gtest is found or not.
ac_defun([gtest_lib_check],
[
dnl provide a flag to enable or disable google test usage.
ac_arg_enable([gtest],
  [as_help_string([--enable-gtest],
                  [enable tests using the google c++ testing framework.
                  (default is enabled.)])],
  [],
  [enable_gtest=])
ac_arg_var([gtest_config],
           [the exact path of google test's 'gtest-config' script.])
ac_arg_var([gtest_cppflags],
           [c-like preprocessor flags for google test.])
ac_arg_var([gtest_cxxflags],
           [c++ compile flags for google test.])
ac_arg_var([gtest_ldflags],
           [linker path and option flags for google test.])
ac_arg_var([gtest_libs],
           [library linking flags for google test.])
ac_arg_var([gtest_version],
           [the version of google test available.])
have_gtest="no"
as_if([test "x${enable_gtest}" != "xno"],
  [ac_msg_checking([for 'gtest-config'])
   as_if([test "x${enable_gtest}" = "xyes"],
     [as_if([test -x "${enable_gtest}/scripts/gtest-config"],
        [gtest_config="${enable_gtest}/scripts/gtest-config"],
        [gtest_config="${enable_gtest}/bin/gtest-config"])
      as_if([test -x "${gtest_config}"], [],
        [ac_msg_result([no])
         ac_msg_error([dnl
unable to locate either a built or installed google test.
the specific location '${enable_gtest}' was provided for a built or installed
google test, but no 'gtest-config' script could be found at this location.])
         ])],
     [ac_path_prog([gtest_config], [gtest-config])])
   as_if([test -x "${gtest_config}"],
     [ac_msg_result([${gtest_config}])
      m4_ifval([$1],
        [_gtest_min_version="--min-version=$1"
         ac_msg_checking([for google test at least version >= $1])],
        [_gtest_min_version="--min-version=0"
         ac_msg_checking([for google test])])
      as_if([${gtest_config} ${_gtest_min_version}],
        [ac_msg_result([yes])
         have_gtest='yes'],
        [ac_msg_result([no])])],
     [ac_msg_result([no])])
   as_if([test "x${have_gtest}" = "xyes"],
     [gtest_cppflags=`${gtest_config} --cppflags`
      gtest_cxxflags=`${gtest_config} --cxxflags`
      gtest_ldflags=`${gtest_config} --ldflags`
      gtest_libs=`${gtest_config} --libs`
      gtest_version=`${gtest_config} --version`
      ac_define([have_gtest],[1],[defined when google test is available.])],
     [as_if([test "x${enable_gtest}" = "xyes"],
        [ac_msg_error([dnl
google test was enabled, but no viable version could be found.])
         ])])])
ac_subst([have_gtest])
am_conditional([have_gtest],[test "x$have_gtest" = "xyes"])
as_if([test "x$have_gtest" = "xyes"],
  [m4_ifval([$2], [$2])],
  [m4_ifval([$3], [$3])])
])
