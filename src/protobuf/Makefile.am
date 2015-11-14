## process this file with automake to produce makefile.in

aclocal_amflags = -i m4

automake_options = foreign

# build . before src so that our all-local and clean-local hooks kicks in at
# the right time.
subdirs = . src

# always include gtest in distributions.
dist_subdirs = $(subdirs) src

# build gtest before we build protobuf tests.  we don't add gtest to subdirs
# because then "make check" would also build and run all of gtest's own tests,
# which takes a lot of time and is generally not useful to us.  also, we don't
# want "make install" to recurse into gtest since we don't want to overwrite
# the installed version of gtest if there is one.
check-local:
	@echo "making lib/libgtest.a lib/libgtest_main.a in gtest"
	@cd gtest && $(make) $(am_makeflags) lib/libgtest.la lib/libgtest_main.la

# we would like to clean gtest when "make clean" is invoked.  but we have to
# be careful because clean-local is also invoked during "make distclean", but
# "make distclean" already recurses into gtest because it's listed among the
# dist_subdirs.  distclean will delete gtest/makefile, so if we then try to
# cd to the directory again and "make clean" it will fail.  so, check that the
# makefile exists before recursing.
clean-local:
	@if test -e gtest/makefile; then \
	  echo "making clean in gtest"; \
	  cd gtest && $(make) $(am_makeflags) clean; \
	fi

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_data = protobuf.pc protobuf-lite.pc

extra_dist =                                                                 \
  autogen.sh                                                                 \
  generate_descriptor_proto.sh                                               \
  readme.txt                                                                 \
  install.txt                                                                \
  copying.txt                                                                \
  contributors.txt                                                           \
  changes.txt                                                                \
  editors/readme.txt                                                         \
  editors/proto.vim                                                          \
  editors/protobuf-mode.el                                                   \
  vsprojects/config.h                                                        \
  vsprojects/extract_includes.bat                                            \
  vsprojects/libprotobuf.vcproj                                              \
  vsprojects/libprotobuf-lite.vcproj                                         \
  vsprojects/libprotoc.vcproj                                                \
  vsprojects/protobuf.sln                                                    \
  vsprojects/protoc.vcproj                                                   \
  vsprojects/readme.txt                                                      \
  vsprojects/test_plugin.vcproj                                              \
  vsprojects/tests.vcproj                                                    \
  vsprojects/lite-test.vcproj                                                \
  vsprojects/convert2008to2005.sh                                            \
  examples/readme.txt                                                        \
  examples/makefile                                                          \
  examples/addressbook.proto                                                 \
  examples/add_person.cc                                                     \
  examples/list_people.cc                                                    \
  examples/addperson.java                                                    \
  examples/listpeople.java                                                   \
  examples/add_person.py                                                     \
  examples/list_people.py                                                    \
  java/src/main/java/com/google/protobuf/abstractmessage.java                \
  java/src/main/java/com/google/protobuf/abstractmessagelite.java            \
  java/src/main/java/com/google/protobuf/abstractparser.java                 \
  java/src/main/java/com/google/protobuf/blockingrpcchannel.java             \
  java/src/main/java/com/google/protobuf/blockingservice.java                \
  java/src/main/java/com/google/protobuf/boundedbytestring.java              \
  java/src/main/java/com/google/protobuf/bytestring.java                     \
  java/src/main/java/com/google/protobuf/codedinputstream.java               \
  java/src/main/java/com/google/protobuf/codedoutputstream.java              \
  java/src/main/java/com/google/protobuf/descriptors.java                    \
  java/src/main/java/com/google/protobuf/dynamicmessage.java                 \
  java/src/main/java/com/google/protobuf/extensionregistry.java              \
  java/src/main/java/com/google/protobuf/extensionregistrylite.java          \
  java/src/main/java/com/google/protobuf/fieldset.java                       \
  java/src/main/java/com/google/protobuf/generatedmessage.java               \
  java/src/main/java/com/google/protobuf/generatedmessagelite.java           \
  java/src/main/java/com/google/protobuf/internal.java                       \
  java/src/main/java/com/google/protobuf/invalidprotocolbufferexception.java \
  java/src/main/java/com/google/protobuf/lazyfield.java                      \
  java/src/main/java/com/google/protobuf/lazystringarraylist.java            \
  java/src/main/java/com/google/protobuf/lazystringlist.java                 \
  java/src/main/java/com/google/protobuf/literalbytestring.java              \
  java/src/main/java/com/google/protobuf/message.java                        \
  java/src/main/java/com/google/protobuf/messagelite.java                    \
  java/src/main/java/com/google/protobuf/messageliteorbuilder.java           \
  java/src/main/java/com/google/protobuf/messageorbuilder.java               \
  java/src/main/java/com/google/protobuf/parser.java                         \
  java/src/main/java/com/google/protobuf/protocolmessageenum.java            \
  java/src/main/java/com/google/protobuf/repeatedfieldbuilder.java           \
  java/src/main/java/com/google/protobuf/ropebytestring.java                 \
  java/src/main/java/com/google/protobuf/rpccallback.java                    \
  java/src/main/java/com/google/protobuf/rpcchannel.java                     \
  java/src/main/java/com/google/protobuf/rpccontroller.java                  \
  java/src/main/java/com/google/protobuf/rpcutil.java                        \
  java/src/main/java/com/google/protobuf/serviceexception.java               \
  java/src/main/java/com/google/protobuf/service.java                        \
  java/src/main/java/com/google/protobuf/singlefieldbuilder.java             \
  java/src/main/java/com/google/protobuf/smallsortedmap.java                 \
  java/src/main/java/com/google/protobuf/textformat.java                     \
  java/src/main/java/com/google/protobuf/uninitializedmessageexception.java  \
  java/src/main/java/com/google/protobuf/unknownfieldset.java                \
  java/src/main/java/com/google/protobuf/unmodifiablelazystringlist.java     \
  java/src/main/java/com/google/protobuf/utf8.java                           \
  java/src/main/java/com/google/protobuf/wireformat.java                     \
  java/src/test/java/com/google/protobuf/abstractmessagetest.java            \
  java/src/test/java/com/google/protobuf/boundedbytestringtest.java          \
  java/src/test/java/com/google/protobuf/bytestringtest.java                 \
  java/src/test/java/com/google/protobuf/codedinputstreamtest.java           \
  java/src/test/java/com/google/protobuf/codedoutputstreamtest.java          \
  java/src/test/java/com/google/protobuf/deprecatedfieldtest.java            \
  java/src/test/java/com/google/protobuf/descriptorstest.java                \
  java/src/test/java/com/google/protobuf/dynamicmessagetest.java             \
  java/src/test/java/com/google/protobuf/forcefieldbuildersprerun.java       \
  java/src/test/java/com/google/protobuf/generatedmessagetest.java           \
  java/src/test/java/com/google/protobuf/isvalidutf8test.java                \
  java/src/test/java/com/google/protobuf/isvalidutf8testutil.java            \
  java/src/test/java/com/google/protobuf/lazystringarraylisttest.java        \
  java/src/test/java/com/google/protobuf/lazystringendtoendtest.java         \
  java/src/test/java/com/google/protobuf/literalbytestringtest.java          \
  java/src/test/java/com/google/protobuf/litetest.java                       \
  java/src/test/java/com/google/protobuf/messagetest.java                    \
  java/src/test/java/com/google/protobuf/nestedbuilderstest.java             \
  java/src/test/java/com/google/protobuf/parsertest.java                     \
  java/src/test/java/com/google/protobuf/repeatedfieldbuildertest.java       \
  java/src/test/java/com/google/protobuf/ropebytestringsubstringtest.java    \
  java/src/test/java/com/google/protobuf/ropebytestringtest.java             \
  java/src/test/java/com/google/protobuf/servicetest.java                    \
  java/src/test/java/com/google/protobuf/singlefieldbuildertest.java         \
  java/src/test/java/com/google/protobuf/smallsortedmaptest.java             \
  java/src/test/java/com/google/protobuf/testbadidentifiers.java             \
  java/src/test/java/com/google/protobuf/testutil.java                       \
  java/src/test/java/com/google/protobuf/textformattest.java                 \
  java/src/test/java/com/google/protobuf/unknownfieldsettest.java            \
  java/src/test/java/com/google/protobuf/unmodifiablelazystringlisttest.java \
  java/src/test/java/com/google/protobuf/wireformattest.java                 \
  java/src/test/java/com/google/protobuf/multiple_files_test.proto           \
  java/src/test/java/com/google/protobuf/nested_builders_test.proto          \
  java/src/test/java/com/google/protobuf/nested_extension_lite.proto         \
  java/src/test/java/com/google/protobuf/nested_extension.proto              \
  java/src/test/java/com/google/protobuf/non_nested_extension_lite.proto     \
  java/src/test/java/com/google/protobuf/non_nested_extension.proto          \
  java/src/test/java/com/google/protobuf/test_bad_identifiers.proto          \
  java/pom.xml                                                               \
  java/readme.txt                                                            \
  python/google/protobuf/internal/generator_test.py                          \
  python/google/protobuf/internal/containers.py                              \
  python/google/protobuf/internal/decoder.py                                 \
  python/google/protobuf/internal/descriptor_database_test.py                \
  python/google/protobuf/internal/descriptor_pool_test.py                    \
  python/google/protobuf/internal/descriptor_test.py                         \
  python/google/protobuf/internal/encoder.py                                 \
  python/google/protobuf/internal/enum_type_wrapper.py                       \
  python/google/protobuf/internal/factory_test1.proto                        \
  python/google/protobuf/internal/factory_test2.proto                        \
  python/google/protobuf/internal/message_cpp_test.py                        \
  python/google/protobuf/internal/message_factory_test.py                    \
  python/google/protobuf/internal/message_listener.py                        \
  python/google/protobuf/internal/message_test.py                            \
  python/google/protobuf/internal/more_extensions.proto                      \
  python/google/protobuf/internal/more_extensions_dynamic.proto              \
  python/google/protobuf/internal/more_messages.proto                        \
  python/google/protobuf/internal/python_message.py                          \
  python/google/protobuf/internal/cpp_message.py                             \
  python/google/protobuf/internal/api_implementation.py                      \
  python/google/protobuf/internal/reflection_test.py                         \
  python/google/protobuf/internal/reflection_cpp_generated_test.py           \
  python/google/protobuf/internal/service_reflection_test.py                 \
  python/google/protobuf/internal/test_bad_identifiers.proto                 \
  python/google/protobuf/internal/test_util.py                               \
  python/google/protobuf/internal/text_format_test.py                        \
  python/google/protobuf/internal/type_checkers.py                           \
  python/google/protobuf/internal/unknown_fields_test.py                     \
  python/google/protobuf/internal/wire_format.py                             \
  python/google/protobuf/internal/wire_format_test.py                        \
  python/google/protobuf/internal/__init__.py                                \
  python/google/protobuf/pyext/python-proto2.cc                              \
  python/google/protobuf/pyext/python_descriptor.cc                          \
  python/google/protobuf/pyext/python_descriptor.h                           \
  python/google/protobuf/pyext/python_protobuf.cc                            \
  python/google/protobuf/pyext/python_protobuf.h                             \
  python/google/protobuf/descriptor.py                                       \
  python/google/protobuf/descriptor_database.py                              \
  python/google/protobuf/descriptor_pool.py                                  \
  python/google/protobuf/message.py                                          \
  python/google/protobuf/message_factory.py                                  \
  python/google/protobuf/reflection.py                                       \
  python/google/protobuf/service.py                                          \
  python/google/protobuf/service_reflection.py                               \
  python/google/protobuf/text_format.py                                      \
  python/google/protobuf/__init__.py                                         \
  python/google/__init__.py                                                  \
  python/ez_setup.py                                                         \
  python/setup.py                                                            \
  python/mox.py                                                              \
  python/stubout.py                                                          \
  python/readme.txt

# deletes all the files generated by autogen.sh.
maintainercleanfiles =   \
  aclocal.m4             \
  config.guess           \
  config.sub             \
  configure              \
  depcomp                \
  install-sh             \
  ltmain.sh              \
  makefile.in            \
  missing                \
  mkinstalldirs          \
  config.h.in            \
  stamp.h.in             \
  m4/ltsugar.m4          \
  m4/libtool.m4          \
  m4/ltversion.m4        \
  m4/lt~obsolete.m4      \
  m4/ltoptions.m4
