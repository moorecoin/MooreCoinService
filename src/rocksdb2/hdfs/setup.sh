export use_hdfs=1
export ld_library_path=$java_home/jre/lib/amd64/server:$java_home/jre/lib/amd64:/usr/lib/hadoop/lib/native

export classpath=
for f in `find /usr/lib/hadoop-hdfs | grep jar`; do export classpath=$classpath:$f; done
for f in `find /usr/lib/hadoop | grep jar`; do export classpath=$classpath:$f; done
for f in `find /usr/lib/hadoop/client | grep jar`; do export classpath=$classpath:$f; done
