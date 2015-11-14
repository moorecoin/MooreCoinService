java -server -d64 -xx:newsize=4m -xx:+aggressiveopts -djava.library.path=.:../ -cp "rocksdbjni.jar:.:./*" org.rocksdb.benchmark.dbbenchmark $@
