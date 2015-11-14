
file (glob source_files *.cpp)
file (glob header_files *.hpp)

init_target (testee_client)

build_executable (${target_name} ${source_files} ${header_files})

link_boost ()
final_target ()

