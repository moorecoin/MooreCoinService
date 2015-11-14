
file (glob source_files *.cpp)
file (glob header_files *.hpp)


if (openssl_found)

init_target (echo_server_tls)

build_executable (${target_name} ${source_files} ${header_files})

link_boost ()
link_openssl()
final_target ()
endif()
