project(lz4 c)
set(cpack_package_description_summary "lz4 compression library")
set(cpack_package_version_major 1)
set(cpack_package_version_minor 5)
set(cpack_package_version_patch r126)
set(version_string	" \"${cpack_package_version_major}.${cpack_package_version_minor}.${cpack_package_version_patch}\" ")
include(cpack)

cmake_minimum_required (version 2.6)
include (checktypesize)
check_type_size("void *" sizeof_void_p)
if( ${sizeof_void_p} strequal  "8" )
    set (cmake_system_processor "64bit")
    message( status "64 bit architecture detected size of void * is " ${sizeof_void_p})
endif()

option(build_tools "build the command line tools" on)
option(build_libs  "build the libraries in addition to the tools" off)

if(unix and build_libs)
    if(cmake_system_processor matches "x86_64")
        add_definitions(-fpic)
    endif(cmake_system_processor matches "x86_64")
endif()

set(lz4_dir ../lib/)
set(prg_dir ../programs/)
set(lz4_srcs_lib ${lz4_dir}lz4.c ${lz4_dir}lz4hc.c ${lz4_dir}lz4.h ${lz4_dir}lz4hc.h)
set(lz4_srcs ${lz4_dir}lz4frame.c ${lz4_dir}xxhash.c ${prg_dir}bench.c ${prg_dir}lz4cli.c ${prg_dir}lz4io.c)

if(build_tools and not build_libs)
    set(lz4_srcs ${lz4_srcs} ${lz4_srcs_lib})
endif()

if(build_tools)
    add_executable(lz4 ${lz4_srcs})
    set_target_properties(lz4 properties compile_definitions disable_lz4c_legacy_options)
    install(targets lz4 runtime destination "bin/")
    add_executable(lz4c ${lz4_srcs})
    install(targets lz4c runtime destination "bin/")
endif()

if(build_libs)    
    add_library(liblz4 ${lz4_srcs_lib})
    
    set_target_properties(liblz4 properties
    output_name lz4
    soversion "${cpack_package_version_major}.${cpack_package_version_minor}"
    )
        
    install(targets liblz4
    library destination lib
    archive destination lib
    )

    install(files
    ${lz4_dir}/lz4.h
    ${lz4_dir}/lz4hc.h
    destination include
    )
    
    if(build_tools)
        target_link_libraries(lz4 liblz4)
        target_link_libraries(lz4c liblz4)
    endif()
endif()


#warnings

add_definitions("-wall")
add_definitions("-wextra")
add_definitions("-wundef")
add_definitions("-wshadow")
add_definitions("-wcast-align")
add_definitions("-wstrict-prototypes")
add_definitions("-std=c99")
add_definitions("-dlz4_version=\"${cpack_package_version_patch}\"")
include_directories (${lz4_dir})



