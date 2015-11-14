native_java_classes = org.rocksdb.rocksdb org.rocksdb.options org.rocksdb.writebatch org.rocksdb.writebatchinternal org.rocksdb.writebatchtest org.rocksdb.writeoptions org.rocksdb.backupabledb org.rocksdb.backupabledboptions org.rocksdb.statistics org.rocksdb.rocksiterator org.rocksdb.vectormemtableconfig org.rocksdb.skiplistmemtableconfig org.rocksdb.hashlinkedlistmemtableconfig org.rocksdb.hashskiplistmemtableconfig org.rocksdb.plaintableconfig org.rocksdb.blockbasedtableconfig org.rocksdb.readoptions org.rocksdb.filter org.rocksdb.bloomfilter org.rocksdb.restoreoptions org.rocksdb.restorebackupabledb org.rocksdb.rocksenv

native_include = ./include
rocksdb_jar = rocksdbjni.jar

clean:
	-find . -name "*.class" -exec rm {} \;
	-find . -name "hs*.log" -exec rm {} \;
	rm -f $(rocksdb_jar)

java:
	javac org/rocksdb/util/*.java org/rocksdb/*.java
	@cp ../history.md ./history-cpp.md
	@rm -f ./history-cpp.md
	javah -d $(native_include) -jni $(native_java_classes)

sample: java
	javac -cp $(rocksdb_jar) rocksdbsample.java
	@rm -rf /tmp/rocksdbjni
	@rm -rf /tmp/rocksdbjni_not_found
	java -ea -djava.library.path=.:../ -cp ".:./*" -xcheck:jni rocksdbsample /tmp/rocksdbjni
	@rm -rf /tmp/rocksdbjni
	@rm -rf /tmp/rocksdbjni_not_found

test: java
	javac org/rocksdb/test/*.java
	java -ea -djava.library.path=.:../ -cp "$(rocksdb_jar):.:./*" org.rocksdb.writebatchtest
	java -ea -djava.library.path=.:../ -cp "$(rocksdb_jar):.:./*" org.rocksdb.test.backupabledbtest
	java -ea -djava.library.path=.:../ -cp "$(rocksdb_jar):.:./*" org.rocksdb.test.optionstest
	java -ea -djava.library.path=.:../ -cp "$(rocksdb_jar):.:./*" org.rocksdb.test.readoptionstest
	java -ea -djava.library.path=.:../ -cp "$(rocksdb_jar):.:./*" org.rocksdb.test.statisticscollectortest

db_bench: java
	javac org/rocksdb/benchmark/*.java
