dirs=broadcast_server_tls chat_client chat_server concurrent_server echo_client echo_server echo_server_tls fuzzing_client fuzzing_server_tls stress_client wsperf

.phony:
all: build

.phony:
build: $(dirs:%=%.build)

.phony:
clean: $(dirs:%=%.clean)

.phony:
%.build:
	$(make) -c $(@:%.build=%)

.phony:
%.clean:
	$(make) -c $(@:%.clean=%) clean
