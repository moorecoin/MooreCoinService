//  copyright (c) 2014, moorecoin, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
package org.rocksdb;

import java.util.*;
import java.io.unsupportedencodingexception;

/**
 * this class mimics the db/write_batch_test.cc in the c++ rocksdb library.
 */
public class writebatchtest {
  static {
    rocksdb.loadlibrary();
  }

  public static void main(string args[]) {
    system.out.println("testing writebatchtest.empty ===");
    empty();

    system.out.println("testing writebatchtest.multiple ===");
    multiple();

    system.out.println("testing writebatchtest.append ===");
    append();

    system.out.println("testing writebatchtest.blob ===");
    blob();

    // the following tests have not yet ported.
    // continue();
    // putgatherslices();

    system.out.println("passed all writebatchtest!");
  }

  static void empty() {
    writebatch batch = new writebatch();
    assert(batch.count() == 0);
  }

  static void multiple() {
    try {
      writebatch batch =  new writebatch();
      batch.put("foo".getbytes("us-ascii"), "bar".getbytes("us-ascii"));
      batch.remove("box".getbytes("us-ascii"));
      batch.put("baz".getbytes("us-ascii"), "boo".getbytes("us-ascii"));
      writebatchinternal.setsequence(batch, 100);
      assert(100 == writebatchinternal.sequence(batch));
      assert(3 == batch.count());
      assert(new string("put(baz, boo)@102" +
                        "delete(box)@101" +
                        "put(foo, bar)@100")
                .equals(new string(getcontents(batch), "us-ascii")));
    } catch (unsupportedencodingexception e) {
      system.err.println(e);
      assert(false);
    }
  }

  static void append() {
    writebatch b1 = new writebatch();
    writebatch b2 = new writebatch();
    writebatchinternal.setsequence(b1, 200);
    writebatchinternal.setsequence(b2, 300);
    writebatchinternal.append(b1, b2);
    assert(getcontents(b1).length == 0);
    assert(b1.count() == 0);
    try {
      b2.put("a".getbytes("us-ascii"), "va".getbytes("us-ascii"));
      writebatchinternal.append(b1, b2);
      assert("put(a, va)@200".equals(new string(getcontents(b1), "us-ascii")));
      assert(1 == b1.count());
      b2.clear();
      b2.put("b".getbytes("us-ascii"), "vb".getbytes("us-ascii"));
      writebatchinternal.append(b1, b2);
      assert(new string("put(a, va)@200" +
                        "put(b, vb)@201")
                .equals(new string(getcontents(b1), "us-ascii")));
      assert(2 == b1.count());
      b2.remove("foo".getbytes("us-ascii"));
      writebatchinternal.append(b1, b2);
      assert(new string("put(a, va)@200" +
                        "put(b, vb)@202" +
                        "put(b, vb)@201" +
                        "delete(foo)@203")
                 .equals(new string(getcontents(b1), "us-ascii")));
      assert(4 == b1.count());
    } catch (unsupportedencodingexception e) {
      system.err.println(e);
      assert(false);
    }
  }

  static void blob() {
    writebatch batch = new writebatch();
    try {
      batch.put("k1".getbytes("us-ascii"), "v1".getbytes("us-ascii"));
      batch.put("k2".getbytes("us-ascii"), "v2".getbytes("us-ascii"));
      batch.put("k3".getbytes("us-ascii"), "v3".getbytes("us-ascii"));
      batch.putlogdata("blob1".getbytes("us-ascii"));
      batch.remove("k2".getbytes("us-ascii"));
      batch.putlogdata("blob2".getbytes("us-ascii"));
      batch.merge("foo".getbytes("us-ascii"), "bar".getbytes("us-ascii"));
      assert(5 == batch.count());
      assert(new string("merge(foo, bar)@4" +
                        "put(k1, v1)@0" +
                        "delete(k2)@3" +
                        "put(k2, v2)@1" +
                        "put(k3, v3)@2")
                .equals(new string(getcontents(batch), "us-ascii")));
    } catch (unsupportedencodingexception e) {
      system.err.println(e);
      assert(false);
    }
  }

  static native byte[] getcontents(writebatch batch);
}
