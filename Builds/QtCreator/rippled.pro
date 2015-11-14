
# ripple protocol buffers

protos = ../../src/ripple_data/protocol/ripple.proto
protos_dir = ../../build/proto

# google protocol buffers support

protobuf_h.name = protobuf header
protobuf_h.input = protos
protobuf_h.output = $${protos_dir}/${qmake_file_base}.pb.h
protobuf_h.depends = ${qmake_file_name}
protobuf_h.commands = protoc --cpp_out=$${protos_dir} --proto_path=${qmake_file_path} ${qmake_file_name}
protobuf_h.variable_out = headers
qmake_extra_compilers += protobuf_h

protobuf_cc.name = protobuf implementation
protobuf_cc.input = protos
protobuf_cc.output = $${protos_dir}/${qmake_file_base}.pb.cc
protobuf_cc.depends = $${protos_dir}/${qmake_file_base}.pb.h
protobuf_cc.commands = $$escape_expand(\\n)
#protobuf_cc.variable_out = sources
qmake_extra_compilers += protobuf_cc

# ripple compilation

destdir = ../../build/qtcreator
objects_dir = ../../build/qtcreator/obj

template = app
config += console thread warn_off
config -= qt gui

defines += _debug

linux-g++:qmake_cxxflags += \
    -wall \
    -wno-sign-compare \
    -wno-char-subscripts \
    -wno-invalid-offsetof \
    -wno-unused-parameter \
    -wformat \
    -o0 \
    -std=c++11 \
    -pthread

includepath += \
    "../../src/leveldb/" \
    "../../src/leveldb/port" \
    "../../src/leveldb/include" \
    $${protos_dir}

other_files += \
#   $$files(../../src/*, true) \
#   $$files(../../src/beast/*) \
#   $$files(../../src/beast/modules/beast_basics/diagnostic/*)
#   $$files(../../src/beast/modules/beast_core/, true)

ui_headers_dir += ../../src/ripple_basics

# ---------
# new style
#
sources += \
    ../../src/ripple/beast/ripple_beast.unity.cpp \
    ../../src/ripple/beast/ripple_beastc.c \
    ../../src/ripple/common/ripple_common.unity.cpp \
    ../../src/ripple/http/ripple_http.unity.cpp \
    ../../src/ripple/json/ripple_json.unity.cpp \
    ../../src/ripple/peerfinder/ripple_peerfinder.unity.cpp \
    ../../src/ripple/radmap/ripple_radmap.unity.cpp \
    ../../src/ripple/resource/ripple_resource.unity.cpp \
    ../../src/ripple/sitefiles/ripple_sitefiles.unity.cpp \
    ../../src/ripple/sslutil/ripple_sslutil.unity.cpp \
    ../../src/ripple/testoverlay/ripple_testoverlay.unity.cpp \
    ../../src/ripple/types/ripple_types.unity.cpp \
    ../../src/ripple/validators/ripple_validators.unity.cpp

# ---------
# old style
#
sources += \
    ../../src/ripple_app/ripple_app.unity.cpp \
    ../../src/ripple_app/ripple_app_pt1.unity.cpp \
    ../../src/ripple_app/ripple_app_pt2.unity.cpp \
    ../../src/ripple_app/ripple_app_pt3.unity.cpp \
    ../../src/ripple_app/ripple_app_pt4.unity.cpp \
    ../../src/ripple_app/ripple_app_pt5.unity.cpp \
    ../../src/ripple_app/ripple_app_pt6.unity.cpp \
    ../../src/ripple_app/ripple_app_pt7.unity.cpp \
    ../../src/ripple_app/ripple_app_pt8.unity.cpp \
    ../../src/ripple_basics/ripple_basics.unity.cpp \
    ../../src/ripple_core/ripple_core.unity.cpp \
    ../../src/ripple_data/ripple_data.unity.cpp \
    ../../src/ripple_hyperleveldb/ripple_hyperleveldb.unity.cpp \
    ../../src/ripple_leveldb/ripple_leveldb.unity.cpp \
    ../../src/ripple_net/ripple_net.unity.cpp \
    ../../src/ripple_overlay/ripple_overlay.unity.cpp \
    ../../src/ripple_rpc/ripple_rpc.unity.cpp \
    ../../src/ripple_websocket/ripple_websocket.unity.cpp

libs += \
    -lboost_date_time-mt\
    -lboost_filesystem-mt \
    -lboost_program_options-mt \
    -lboost_regex-mt \
    -lboost_system-mt \
    -lboost_thread-mt \
    -lboost_random-mt \
    -lprotobuf \
    -lssl \
    -lrt
