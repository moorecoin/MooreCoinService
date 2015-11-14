
############ setup project and cmake

# project name
project (websocketpp)

# minimum cmake requirement. we should require a quite recent
# cmake for the dependency find macros etc. to be up to date.
cmake_minimum_required (version 2.6)

set (websocketpp_major_version 0)
set (websocketpp_minor_version 4)
set (websocketpp_patch_version 0)
set (websocketpp_version ${websocketpp_major_version}.${websocketpp_minor_version}.${websocketpp_patch_version})

set(install_include_dir include cache path "installation directory for header files")
if (win32 and not cygwin)
  set (def_install_cmake_dir cmake)
else ()
  set (def_install_cmake_dir lib/cmake/websocketpp)
endif ()
set (install_cmake_dir ${def_install_cmake_dir} cache path "installation directory for cmake files")

# make relative paths absolute (needed later on)
foreach (p include cmake)
  set (var install_${p}_dir)
  if (not is_absolute "${${var}}")
    set (${var} "${cmake_install_prefix}/${${var}}")
  endif ()
endforeach ()

# set cmake library search policy
if (command cmake_policy)
    cmake_policy (set cmp0003 new)
    cmake_policy (set cmp0005 new)
endif ()

# disable unnecessary build types
set (cmake_configuration_types "release;relwithdebinfo;debug" cache string "configurations" force)

# include our cmake macros
set (cmake_module_path ${cmake_module_path} ${cmake_current_source_dir}/cmake)
include (cmakehelpers)

############ paths

set (websocketpp_root ${cmake_current_source_dir})
set (websocketpp_include ${websocketpp_root}/websocketpp)
set (websocketpp_build_root ${cmake_current_binary_dir})
set (websocketpp_bin ${websocketpp_build_root}/bin)
set (websocketpp_lib ${websocketpp_build_root}/lib)

# cmake install step prefix. i assume linux users want the prefix to
# be the default /usr or /usr/local so this is only adjusted on windows.
# - windows: build the install project in your solution file.
# - linux/osx: make install.
if (msvc)
    set (cmake_install_prefix "${websocketpp_root}/install")
endif ()

############  build customization

# override from command line "cmake -d<option>=true/false/0/1/on/off"
option (enable_cpp11 "build websocketpp with cpp11 features enabled." true)
option (build_examples "build websocketpp examples." false)
option (build_tests "build websocketpp tests." false)

if (build_tests or build_examples)

    ############ compiler specific setup

    set (websocketpp_platform_libs "")
    set (websocketpp_platform_tsl_libs "")
    set (websocketpp_boost_libs "")

    # vc9 and c++11 reasoning
    if (enable_cpp11 and msvc and msvc90)
        message("* detected visual studio 9 2008, disabling c++11 support.")
        set (enable_cpp11 false)
    endif ()

    # detect clang. not officially reported by cmake.
    execute_process(command "${cmake_cxx_compiler}" "-v" error_variable cxx_ver_stderr)
    if ("${cxx_ver_stderr}" matches ".*clang.*")
        set (cmake_compiler_is_clangxx 1)
    endif ()

    # c++11 defines
    if (enable_cpp11)
        if (msvc)
            add_definitions (-d_websocketpp_cpp11_functional_)
            add_definitions (-d_websocketpp_cpp11_system_error_)
            add_definitions (-d_websocketpp_cpp11_random_device_)
            add_definitions (-d_websocketpp_cpp11_memory_)
        else()
            add_definitions (-d_websocketpp_cpp11_stl_)
        endif()
    endif ()

    # visual studio
    if (msvc)
        set (websocketpp_boost_libs system thread)
        set (cmake_cxx_flags_release "${cmake_cxx_flags_release} /gl /gy /gf /ox /ob2 /ot /oi /mp /arch:sse2 /fp:fast")
        set (cmake_shared_linker_flags_release "${cmake_shared_linker_flags_release} /ltcg /incremental:no /opt:ref /opt:icf")
        add_definitions (/w3 /wd4996 /wd4995 /wd4355)
        add_definitions (-dunicode -d_unicode)
        add_definitions (-d_crt_secure_no_warnings -d_scl_secure_no_warnings)
        add_definitions (-dnominmax)
    endif ()

    # g++
    if (cmake_compiler_is_gnucxx)
        set (websocketpp_platform_libs pthread rt)
        set (websocketpp_platform_tsl_libs ssl crypto)
        set (websocketpp_boost_libs system thread)
        set (cmake_cxx_flags "${cmake_cxx_flags} -std=c++0x")
        if (not apple)
            add_definitions (-dndebug -wall -wcast-align) # todo: should we use cmake_c_flags for these?
        endif ()

        # try to detect version. note: not tested!
        execute_process (command ${cmake_cxx_compiler} "-dumpversion" output_variable gcc_version)
        if ("${gcc_version}" strgreater "4.4.0")
            message("* c++11 support partially enabled due to gcc version ${gcc_version}")
            set (websocketpp_boost_libs system thread)
        endif ()
    endif ()

    # clang
    if (cmake_compiler_is_clangxx)
        if (not apple)
            set (websocketpp_platform_libs pthread rt)
        else()
            set (websocketpp_platform_libs pthread)
        endif()
        set (websocketpp_platform_tsl_libs ssl crypto)
        set (websocketpp_boost_libs system thread)
        set (cmake_cxx_flags ${cmake_cxx_flags} "-std=c++0x -stdlib=libc++") # todo: is libc++ really needed here?
        if (not apple)
            add_definitions (-dndebug -wall -wno-padded) # todo: should we use cmake_c_flags for these?
        endif ()
    endif ()

    # osx, can override above.
    if (apple)
        add_definitions (-dndebug -wall)
    endif ()

    if (build_examples)
        list (append websocketpp_boost_libs random)
    endif()

    ############ dependencies

    # set boost_root env variable or pass with cmake -dboost_root=path.
    # boost_root can also be defined by a previous run from cmake cache.
    if (not "$env{boost_root_cpp11}" strequal "")
        # scons documentation for boost_root_cpp11:
        # "look for optional second boostroot compiled with clang's libc++ stl library
        # this prevents warnings/errors when linking code built with two different
        # incompatible stl libraries."
        file (to_cmake_path "$env{boost_root_cpp11}" boost_root)
        set (boost_root ${boost_root} cache path "boost_root dependency path" force)
    endif ()
    if ("${boost_root}" strequal "")
        file (to_cmake_path "$env{boost_root}" boost_root)
        # cache boost_root for runs that do not define $env{boost_root}.
        set (boost_root ${boost_root} cache path "boost_root dependency path" force)
    endif ()

    message ("* configuring boost")
    message (status "-- using boost_root")
    message (status "       " ${boost_root})

    if (msvc)
        set (boost_use_multithreaded true)
        set (boost_use_static_libs true)
    else ()
        set (boost_use_multithreaded false)
        set (boost_use_static_libs false)
    endif ()

    set (boost_find_required true)
    set (boost_find_quietly true)
    set (boost_debug false)
    set (boost_use_multithreaded true)
    set (boost_additional_versions "1.39.0" "1.40.0" "1.41.0" "1.42.0" "1.43.0" "1.44.0" "1.46.1") # todo: someone who knows better spesify these!

    find_package (boost 1.39.0 components "${websocketpp_boost_libs}")

    if (boost_found)
        # boost is a project wide global dependency.
        include_directories (${boost_include_dirs})
        link_directories (${boost_library_dirs})

        # pretty print status
        message (status "-- include directories")
        foreach (include_dir ${boost_include_dirs})
            message (status "       " ${include_dir})
        endforeach ()
        message (status "-- library directories")
        foreach (library_dir ${boost_library_dirs})
            message (status "       " ${library_dir})
        endforeach ()
        message (status "-- libraries")
        foreach (boost_lib ${boost_libraries})
            message (status "       " ${boost_lib})
        endforeach ()
        message ("")
    else ()
        message (fatal_error "failed to find required dependency: boost")
    endif ()

    find_package(openssl)
endif()

############ add projects

# add main library
add_subdirectory (websocketpp)

# add examples
if (build_examples)
    add_subdirectory (examples)
endif ()

# add tests
if (build_tests)
    add_subdirectory (test)
endif ()

print_used_build_config()

export (package websocketpp)

configure_file (websocketpp-config.cmake.in "${project_binary_dir}${cmake_files_directory}/websocketpp-config.cmake" @only)
configure_file (websocketpp-configversion.cmake.in "${project_binary_dir}${cmake_files_directory}/websocketpp-configversion.cmake" @only)

# install the websocketpp-config.cmake and websocketpp-configversion.cmake
install (files
  "${project_binary_dir}${cmake_files_directory}/websocketpp-config.cmake"
  "${project_binary_dir}${cmake_files_directory}/websocketpp-configversion.cmake"
  destination "${install_cmake_dir}" component dev)

