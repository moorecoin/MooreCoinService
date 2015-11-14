// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

import java.util.arrays;
import java.util.list;
import java.util.map;
import java.util.arraylist;
import org.rocksdb.*;
import org.rocksdb.util.sizeunit;
import java.io.ioexception;

public class rocksdbsample {
  static {
    rocksdb.loadlibrary();
  }

  public static void main(string[] args) {
    if (args.length < 1) {
      system.out.println("usage: rocksdbsample db_path");
      return;
    }
    string db_path = args[0];
    string db_path_not_found = db_path + "_not_found";

    system.out.println("rocksdbsample");
    rocksdb db = null;
    options options = new options();
    try {
      db = rocksdb.open(options, db_path_not_found);
      assert(false);
    } catch (rocksdbexception e) {
      system.out.format("caught the expceted exception -- %s\n", e);
      assert(db == null);
    }

    options.setcreateifmissing(true)
        .createstatistics()
        .setwritebuffersize(8 * sizeunit.kb)
        .setmaxwritebuffernumber(3)
        .setmaxbackgroundcompactions(10)
        .setcompressiontype(compressiontype.snappy_compression)
        .setcompactionstyle(compactionstyle.universal);
    statistics stats = options.statisticsptr();

    assert(options.createifmissing() == true);
    assert(options.writebuffersize() == 8 * sizeunit.kb);
    assert(options.maxwritebuffernumber() == 3);
    assert(options.maxbackgroundcompactions() == 10);
    assert(options.compressiontype() == compressiontype.snappy_compression);
    assert(options.compactionstyle() == compactionstyle.universal);

    assert(options.memtablefactoryname().equals("skiplistfactory"));
    options.setmemtableconfig(
        new hashskiplistmemtableconfig()
            .setheight(4)
            .setbranchingfactor(4)
            .setbucketcount(2000000));
    assert(options.memtablefactoryname().equals("hashskiplistrepfactory"));

    options.setmemtableconfig(
        new hashlinkedlistmemtableconfig()
            .setbucketcount(100000));
    assert(options.memtablefactoryname().equals("hashlinkedlistrepfactory"));

    options.setmemtableconfig(
        new vectormemtableconfig().setreservedsize(10000));
    assert(options.memtablefactoryname().equals("vectorrepfactory"));

    options.setmemtableconfig(new skiplistmemtableconfig());
    assert(options.memtablefactoryname().equals("skiplistfactory"));

    options.settableformatconfig(new plaintableconfig());
    assert(options.tablefactoryname().equals("plaintable"));

    blockbasedtableconfig table_options = new blockbasedtableconfig();
    table_options.setblockcachesize(64 * sizeunit.kb)
                 .setfilterbitsperkey(10)
                 .setcachenumshardbits(6);
    assert(table_options.blockcachesize() == 64 * sizeunit.kb);
    assert(table_options.cachenumshardbits() == 6);
    options.settableformatconfig(table_options);
    assert(options.tablefactoryname().equals("blockbasedtable"));

    try {
      db = rocksdb.open(options, db_path_not_found);
      db.put("hello".getbytes(), "world".getbytes());
      byte[] value = db.get("hello".getbytes());
      assert("world".equals(new string(value)));
    } catch (rocksdbexception e) {
      system.out.format("[error] caught the unexpceted exception -- %s\n", e);
      assert(db == null);
      assert(false);
    }
    // be sure to release the c++ pointer
    db.close();

    readoptions readoptions = new readoptions();
    readoptions.setfillcache(false);

    try {
      db = rocksdb.open(options, db_path);
      db.put("hello".getbytes(), "world".getbytes());
      byte[] value = db.get("hello".getbytes());
      system.out.format("get('hello') = %s\n",
          new string(value));

      for (int i = 1; i <= 9; ++i) {
        for (int j = 1; j <= 9; ++j) {
          db.put(string.format("%dx%d", i, j).getbytes(),
                 string.format("%d", i * j).getbytes());
        }
      }

      for (int i = 1; i <= 9; ++i) {
        for (int j = 1; j <= 9; ++j) {
          system.out.format("%s ", new string(db.get(
              string.format("%dx%d", i, j).getbytes())));
        }
        system.out.println("");
      }

      value = db.get("1x1".getbytes());
      assert(value != null);
      value = db.get("world".getbytes());
      assert(value == null);
      value = db.get(readoptions, "world".getbytes());
      assert(value == null);

      byte[] testkey = "asdf".getbytes();
      byte[] testvalue =
          "asdfghjkl;'?><mnbvcxzqwertyuiop{+_)(*&^%$#@".getbytes();
      db.put(testkey, testvalue);
      byte[] testresult = db.get(testkey);
      assert(testresult != null);
      assert(arrays.equals(testvalue, testresult));
      assert(new string(testvalue).equals(new string(testresult)));
      testresult = db.get(readoptions, testkey);
      assert(testresult != null);
      assert(arrays.equals(testvalue, testresult));
      assert(new string(testvalue).equals(new string(testresult)));

      byte[] insufficientarray = new byte[10];
      byte[] enougharray = new byte[50];
      int len;
      len = db.get(testkey, insufficientarray);
      assert(len > insufficientarray.length);
      len = db.get("asdfjkl;".getbytes(), enougharray);
      assert(len == rocksdb.not_found);
      len = db.get(testkey, enougharray);
      assert(len == testvalue.length);

      len = db.get(readoptions, testkey, insufficientarray);
      assert(len > insufficientarray.length);
      len = db.get(readoptions, "asdfjkl;".getbytes(), enougharray);
      assert(len == rocksdb.not_found);
      len = db.get(readoptions, testkey, enougharray);
      assert(len == testvalue.length);

      db.remove(testkey);
      len = db.get(testkey, enougharray);
      assert(len == rocksdb.not_found);

      // repeat the test with writeoptions
      writeoptions writeopts = new writeoptions();
      writeopts.setsync(true);
      writeopts.setdisablewal(true);
      db.put(writeopts, testkey, testvalue);
      len = db.get(testkey, enougharray);
      assert(len == testvalue.length);
      assert(new string(testvalue).equals(
          new string(enougharray, 0, len)));
      writeopts.dispose();

      try {
        for (tickertype statstype : tickertype.values()) {
          stats.gettickercount(statstype);
        }
        system.out.println("gettickercount() passed.");
      } catch (exception e) {
        system.out.println("failed in call to gettickercount()");
        assert(false); //should never reach here.
      }

      try {
        for (histogramtype histogramtype : histogramtype.values()) {
          histogramdata data = stats.gehistogramdata(histogramtype);
        }
        system.out.println("gehistogramdata() passed.");
      } catch (exception e) {
        system.out.println("failed in call to gehistogramdata()");
        assert(false); //should never reach here.
      }

      rocksiterator iterator = db.newiterator();

      boolean seektofirstpassed = false;
      for (iterator.seektofirst(); iterator.isvalid(); iterator.next()) {
        iterator.status();
        assert(iterator.key() != null);
        assert(iterator.value() != null);
        seektofirstpassed = true;
      }
      if(seektofirstpassed) {
        system.out.println("iterator seektofirst tests passed.");
      }

      boolean seektolastpassed = false;
      for (iterator.seektolast(); iterator.isvalid(); iterator.prev()) {
        iterator.status();
        assert(iterator.key() != null);
        assert(iterator.value() != null);
        seektolastpassed = true;
      }

      if(seektolastpassed) {
        system.out.println("iterator seektolastpassed tests passed.");
      }

      iterator.seektofirst();
      iterator.seek(iterator.key());
      assert(iterator.key() != null);
      assert(iterator.value() != null);

      system.out.println("iterator seek test passed.");

      iterator.dispose();
      system.out.println("iterator tests passed.");

      iterator = db.newiterator();
      list<byte[]> keys = new arraylist<byte[]>();
      for (iterator.seektolast(); iterator.isvalid(); iterator.prev()) {
        keys.add(iterator.key());
      }
      iterator.dispose();

      map<byte[], byte[]> values = db.multiget(keys);
      assert(values.size() == keys.size());
      for(byte[] value1 : values.values()) {
        assert(value1 != null);
      }

      values = db.multiget(new readoptions(), keys);
      assert(values.size() == keys.size());
      for(byte[] value1 : values.values()) {
        assert(value1 != null);
      }
    } catch (rocksdbexception e) {
      system.err.println(e);
    }
    if (db != null) {
      db.close();
    }
    // be sure to dispose c++ pointers
    options.dispose();
    readoptions.dispose();
  }
}
