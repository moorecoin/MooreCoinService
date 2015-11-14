// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
/**
 * copyright (c) 2011 the original author or authors.
 * see the notice.md file distributed with this work for additional
 * information regarding copyright ownership.
 *
 * licensed under the apache license, version 2.0 (the "license");
 * you may not use this file except in compliance with the license.
 * you may obtain a copy of the license at
 *
 *     http://www.apache.org/licenses/license-2.0
 *
 * unless required by applicable law or agreed to in writing, software
 * distributed under the license is distributed on an "as is" basis,
 * without warranties or conditions of any kind, either express or implied.
 * see the license for the specific language governing permissions and
 * limitations under the license.
 */
package org.rocksdb.benchmark;

import java.lang.runnable;
import java.lang.math;
import java.io.file;
import java.nio.bytebuffer;
import java.util.collection;
import java.util.date;
import java.util.enummap;
import java.util.list;
import java.util.map;
import java.util.random;
import java.util.concurrent.timeunit;
import java.util.arrays;
import java.util.arraylist;
import java.util.concurrent.callable;
import java.util.concurrent.executors;
import java.util.concurrent.executorservice;
import java.util.concurrent.future;
import java.util.concurrent.timeunit;
import org.rocksdb.*;
import org.rocksdb.util.sizeunit;

class stats {
  int id_;
  long start_;
  long finish_;
  double seconds_;
  long done_;
  long found_;
  long lastoptime_;
  long nextreport_;
  long bytes_;
  stringbuilder message_;
  boolean excludefrommerge_;

  // todo(yhchiang): use the following arguments:
  //   (long)flag.stats_interval
  //   (integer)flag.stats_per_interval

  stats(int id) {
    id_ = id;
    nextreport_ = 100;
    done_ = 0;
    bytes_ = 0;
    seconds_ = 0;
    start_ = system.nanotime();
    lastoptime_ = start_;
    finish_ = start_;
    found_ = 0;
    message_ = new stringbuilder("");
    excludefrommerge_ = false;
  }

  void merge(final stats other) {
    if (other.excludefrommerge_) {
      return;
    }

    done_ += other.done_;
    found_ += other.found_;
    bytes_ += other.bytes_;
    seconds_ += other.seconds_;
    if (other.start_ < start_) start_ = other.start_;
    if (other.finish_ > finish_) finish_ = other.finish_;

    // just keep the messages from one thread
    if (message_.length() == 0) {
      message_ = other.message_;
    }
  }

  void stop() {
    finish_ = system.nanotime();
    seconds_ = (double) (finish_ - start_) / 1000000;
  }

  void addmessage(string msg) {
    if (message_.length() > 0) {
      message_.append(" ");
    }
    message_.append(msg);
  }

  void setid(int id) { id_ = id; }
  void setexcludefrommerge() { excludefrommerge_ = true; }

  void finishedsingleop(int bytes) {
    done_++;
    lastoptime_ = system.nanotime();
    bytes_ += bytes;
    if (done_ >= nextreport_) {
      if (nextreport_ < 1000) {
        nextreport_ += 100;
      } else if (nextreport_ < 5000) {
        nextreport_ += 500;
      } else if (nextreport_ < 10000) {
        nextreport_ += 1000;
      } else if (nextreport_ < 50000) {
        nextreport_ += 5000;
      } else if (nextreport_ < 100000) {
        nextreport_ += 10000;
      } else if (nextreport_ < 500000) {
        nextreport_ += 50000;
      } else {
        nextreport_ += 100000;
      }
      system.err.printf("... task %s finished %d ops%30s\r", id_, done_, "");
    }
  }

  void report(string name) {
    // pretend at least one op was done in case we are running a benchmark
    // that does not call finishedsingleop().
    if (done_ < 1) done_ = 1;

    stringbuilder extra = new stringbuilder("");
    if (bytes_ > 0) {
      // rate is computed on actual elapsed time, not the sum of per-thread
      // elapsed times.
      double elapsed = (finish_ - start_) * 1e-6;
      extra.append(string.format("%6.1f mb/s", (bytes_ / 1048576.0) / elapsed));
    }
    extra.append(message_.tostring());
    double elapsed = (finish_ - start_) * 1e-6;
    double throughput = (double) done_ / elapsed;

    system.out.format("%-12s : %11.3f micros/op %d ops/sec;%s%s\n",
            name, elapsed * 1e6 / done_,
            (long) throughput, (extra.length() == 0 ? "" : " "), extra.tostring());
  }
}

public class dbbenchmark {
  enum order {
    sequential,
    random
  }

  enum dbstate {
    fresh,
    existing
  }

  enum compressiontype {
    none,
    snappy,
    zlib,
    bzip2,
    lz4,
    lz4hc
  }

  static {
    rocksdb.loadlibrary();
  }

  abstract class benchmarktask implements callable<stats> {
    // todo(yhchiang): use (integer)flag.perf_level.
    public benchmarktask(
        int tid, long randseed, long numentries, long keyrange) {
      tid_ = tid;
      rand_ = new random(randseed + tid * 1000);
      numentries_ = numentries;
      keyrange_ = keyrange;
      stats_ = new stats(tid);
    }

    @override public stats call() throws rocksdbexception {
      stats_.start_ = system.nanotime();
      runtask();
      stats_.finish_ = system.nanotime();
      return stats_;
    }

    abstract protected void runtask() throws rocksdbexception;

    protected int tid_;
    protected random rand_;
    protected long numentries_;
    protected long keyrange_;
    protected stats stats_;

    protected void getfixedkey(byte[] key, long sn) {
      generatekeyfromlong(key, sn);
    }

    protected void getrandomkey(byte[] key, long range) {
      generatekeyfromlong(key, math.abs(rand_.nextlong() % range));
    }
  }

  abstract class writetask extends benchmarktask {
    public writetask(
        int tid, long randseed, long numentries, long keyrange,
        writeoptions writeopt, long entriesperbatch) {
      super(tid, randseed, numentries, keyrange);
      writeopt_ = writeopt;
      entriesperbatch_ = entriesperbatch;
      maxwritespersecond_ = -1;
    }

    public writetask(
        int tid, long randseed, long numentries, long keyrange,
        writeoptions writeopt, long entriesperbatch, long maxwritespersecond) {
      super(tid, randseed, numentries, keyrange);
      writeopt_ = writeopt;
      entriesperbatch_ = entriesperbatch;
      maxwritespersecond_ = maxwritespersecond;
    }

    @override public void runtask() throws rocksdbexception {
      if (numentries_ != dbbenchmark.this.num_) {
        stats_.message_.append(string.format(" (%d ops)", numentries_));
      }
      byte[] key = new byte[keysize_];
      byte[] value = new byte[valuesize_];

      try {
        if (entriesperbatch_ == 1) {
          for (long i = 0; i < numentries_; ++i) {
            getkey(key, i, keyrange_);
            dbbenchmark.this.gen_.generate(value);
            db_.put(writeopt_, key, value);
            stats_.finishedsingleop(keysize_ + valuesize_);
            writeratecontrol(i);
            if (isfinished()) {
              return;
            }
          }
        } else {
          for (long i = 0; i < numentries_; i += entriesperbatch_) {
            writebatch batch = new writebatch();
            for (long j = 0; j < entriesperbatch_; j++) {
              getkey(key, i + j, keyrange_);
              dbbenchmark.this.gen_.generate(value);
              db_.put(writeopt_, key, value);
              stats_.finishedsingleop(keysize_ + valuesize_);
            }
            db_.write(writeopt_, batch);
            batch.dispose();
            writeratecontrol(i);
            if (isfinished()) {
              return;
            }
          }
        }
      } catch (interruptedexception e) {
        // thread has been terminated.
      }
    }

    protected void writeratecontrol(long writecount)
        throws interruptedexception {
      if (maxwritespersecond_ <= 0) return;
      long mininterval =
          writecount * timeunit.seconds.tonanos(1) / maxwritespersecond_;
      long interval = system.nanotime() - stats_.start_;
      if (mininterval - interval > timeunit.milliseconds.tonanos(1)) {
        timeunit.nanoseconds.sleep(mininterval - interval);
      }
    }

    abstract protected void getkey(byte[] key, long id, long range);
    protected writeoptions writeopt_;
    protected long entriesperbatch_;
    protected long maxwritespersecond_;
  }

  class writesequentialtask extends writetask {
    public writesequentialtask(
        int tid, long randseed, long numentries, long keyrange,
        writeoptions writeopt, long entriesperbatch) {
      super(tid, randseed, numentries, keyrange,
            writeopt, entriesperbatch);
    }
    public writesequentialtask(
        int tid, long randseed, long numentries, long keyrange,
        writeoptions writeopt, long entriesperbatch,
        long maxwritespersecond) {
      super(tid, randseed, numentries, keyrange,
            writeopt, entriesperbatch,
            maxwritespersecond);
    }
    @override protected void getkey(byte[] key, long id, long range) {
      getfixedkey(key, id);
    }
  }

  class writerandomtask extends writetask {
    public writerandomtask(
        int tid, long randseed, long numentries, long keyrange,
        writeoptions writeopt, long entriesperbatch) {
      super(tid, randseed, numentries, keyrange,
            writeopt, entriesperbatch);
    }
    public writerandomtask(
        int tid, long randseed, long numentries, long keyrange,
        writeoptions writeopt, long entriesperbatch,
        long maxwritespersecond) {
      super(tid, randseed, numentries, keyrange,
            writeopt, entriesperbatch,
            maxwritespersecond);
    }
    @override protected void getkey(byte[] key, long id, long range) {
      getrandomkey(key, range);
    }
  }

  class writeuniquerandomtask extends writetask {
    static final int max_buffer_size = 10000000;
    public writeuniquerandomtask(
        int tid, long randseed, long numentries, long keyrange,
        writeoptions writeopt, long entriesperbatch) {
      super(tid, randseed, numentries, keyrange,
            writeopt, entriesperbatch);
      initrandomkeysequence();
    }
    public writeuniquerandomtask(
        int tid, long randseed, long numentries, long keyrange,
        writeoptions writeopt, long entriesperbatch,
        long maxwritespersecond) {
      super(tid, randseed, numentries, keyrange,
            writeopt, entriesperbatch,
            maxwritespersecond);
      initrandomkeysequence();
    }
    @override protected void getkey(byte[] key, long id, long range) {
      generatekeyfromlong(key, nextuniquerandom());
    }

    protected void initrandomkeysequence() {
      buffersize_ = max_buffer_size;
      if (buffersize_ > keyrange_) {
        buffersize_ = (int) keyrange_;
      }
      currentkeycount_ = buffersize_;
      keybuffer_ = new long[max_buffer_size];
      for (int k = 0; k < buffersize_; ++k) {
        keybuffer_[k] = k;
      }
    }

    /**
     * semi-randomly return the next unique key.  it is guaranteed to be
     * fully random if keyrange_ <= max_buffer_size.
     */
    long nextuniquerandom() {
      if (buffersize_ == 0) {
        system.err.println("buffersize_ == 0.");
        return 0;
      }
      int r = rand_.nextint(buffersize_);
      // randomly pick one from the keybuffer
      long randkey = keybuffer_[r];
      if (currentkeycount_ < keyrange_) {
        // if we have not yet inserted all keys, insert next new key to [r].
        keybuffer_[r] = currentkeycount_++;
      } else {
        // move the last element to [r] and decrease the size by 1.
        keybuffer_[r] = keybuffer_[--buffersize_];
      }
      return randkey;
    }

    int buffersize_;
    long currentkeycount_;
    long[] keybuffer_;
  }

  class readrandomtask extends benchmarktask {
    public readrandomtask(
        int tid, long randseed, long numentries, long keyrange) {
      super(tid, randseed, numentries, keyrange);
    }
    @override public void runtask() throws rocksdbexception {
      byte[] key = new byte[keysize_];
      byte[] value = new byte[valuesize_];
      for (long i = 0; i < numentries_; i++) {
        getrandomkey(key, keyrange_);
        int len = db_.get(key, value);
        if (len != rocksdb.not_found) {
          stats_.found_++;
          stats_.finishedsingleop(keysize_ + valuesize_);
        } else {
          stats_.finishedsingleop(keysize_);
        }
        if (isfinished()) {
          return;
        }
      }
    }
  }

  class readsequentialtask extends benchmarktask {
    public readsequentialtask(
        int tid, long randseed, long numentries, long keyrange) {
      super(tid, randseed, numentries, keyrange);
    }
    @override public void runtask() throws rocksdbexception {
      rocksiterator iter = db_.newiterator();
      long i;
      for (iter.seektofirst(), i = 0;
           iter.isvalid() && i < numentries_;
           iter.next(), ++i) {
        stats_.found_++;
        stats_.finishedsingleop(iter.key().length + iter.value().length);
        if (isfinished()) {
          return;
        }
      }
    }
  }

  public dbbenchmark(map<flag, object> flags) throws exception {
    benchmarks_ = (list<string>) flags.get(flag.benchmarks);
    num_ = (integer) flags.get(flag.num);
    threadnum_ = (integer) flags.get(flag.threads);
    reads_ = (integer) (flags.get(flag.reads) == null ?
        flags.get(flag.num) : flags.get(flag.reads));
    keysize_ = (integer) flags.get(flag.key_size);
    valuesize_ = (integer) flags.get(flag.value_size);
    compressionratio_ = (double) flags.get(flag.compression_ratio);
    useexisting_ = (boolean) flags.get(flag.use_existing_db);
    randseed_ = (long) flags.get(flag.seed);
    databasedir_ = (string) flags.get(flag.db);
    writesperseconds_ = (integer) flags.get(flag.writes_per_second);
    memtable_ = (string) flags.get(flag.memtablerep);
    maxwritebuffernumber_ = (integer) flags.get(flag.max_write_buffer_number);
    prefixsize_ = (integer) flags.get(flag.prefix_size);
    keysperprefix_ = (integer) flags.get(flag.keys_per_prefix);
    hashbucketcount_ = (long) flags.get(flag.hash_bucket_count);
    useplaintable_ = (boolean) flags.get(flag.use_plain_table);
    flags_ = flags;
    finishlock_ = new object();
    // options.setprefixsize((integer)flags_.get(flag.prefix_size));
    // options.setkeysperprefix((long)flags_.get(flag.keys_per_prefix));
    compressiontype_ = (string) flags.get(flag.compression_type);
    compression_ = compressiontype.none;
    try {
      if (compressiontype_.equals("snappy")) {
        system.loadlibrary("snappy");
      } else if (compressiontype_.equals("zlib")) {
        system.loadlibrary("z");
      } else if (compressiontype_.equals("bzip2")) {
        system.loadlibrary("bzip2");
      } else if (compressiontype_.equals("lz4")) {
        system.loadlibrary("lz4");
      } else if (compressiontype_.equals("lz4hc")) {
        system.loadlibrary("lz4hc");
      }
    } catch (unsatisfiedlinkerror e) {
      system.err.format("unable to load %s library:%s%n" +
                        "no compression is used.%n",
          compressiontype_, e.tostring());
      compressiontype_ = "none";
    }
    gen_ = new randomgenerator(randseed_, compressionratio_);
  }

  private void preparereadoptions(readoptions options) {
    options.setverifychecksums((boolean)flags_.get(flag.verify_checksum));
    options.settailing((boolean)flags_.get(flag.use_tailing_iterator));
  }

  private void preparewriteoptions(writeoptions options) {
    options.setsync((boolean)flags_.get(flag.sync));
    options.setdisablewal((boolean)flags_.get(flag.disable_wal));
  }

  private void prepareoptions(options options) {
    if (!useexisting_) {
      options.setcreateifmissing(true);
    } else {
      options.setcreateifmissing(false);
    }
    if (memtable_.equals("skip_list")) {
      options.setmemtableconfig(new skiplistmemtableconfig());
    } else if (memtable_.equals("vector")) {
      options.setmemtableconfig(new vectormemtableconfig());
    } else if (memtable_.equals("hash_linkedlist")) {
      options.setmemtableconfig(
          new hashlinkedlistmemtableconfig()
              .setbucketcount(hashbucketcount_));
      options.usefixedlengthprefixextractor(prefixsize_);
    } else if (memtable_.equals("hash_skiplist") ||
               memtable_.equals("prefix_hash")) {
      options.setmemtableconfig(
          new hashskiplistmemtableconfig()
              .setbucketcount(hashbucketcount_));
      options.usefixedlengthprefixextractor(prefixsize_);
    } else {
      system.err.format(
          "unable to detect the specified memtable, " +
          "use the default memtable factory %s%n",
          options.memtablefactoryname());
    }
    if (useplaintable_) {
      options.settableformatconfig(
          new plaintableconfig().setkeysize(keysize_));
    } else {
      blockbasedtableconfig table_options = new blockbasedtableconfig();
      table_options.setblocksize((long)flags_.get(flag.block_size))
                   .setblockcachesize((long)flags_.get(flag.cache_size))
                   .setfilterbitsperkey((integer)flags_.get(flag.bloom_bits))
                   .setcachenumshardbits((integer)flags_.get(flag.cache_numshardbits));
      options.settableformatconfig(table_options);
    }
    options.setwritebuffersize(
        (long)flags_.get(flag.write_buffer_size));
    options.setmaxwritebuffernumber(
        (integer)flags_.get(flag.max_write_buffer_number));
    options.setmaxbackgroundcompactions(
        (integer)flags_.get(flag.max_background_compactions));
    options.getenv().setbackgroundthreads(
        (integer)flags_.get(flag.max_background_compactions));
    options.setmaxbackgroundflushes(
        (integer)flags_.get(flag.max_background_flushes));
    options.setmaxopenfiles(
        (integer)flags_.get(flag.open_files));
    options.settablecacheremovescancountlimit(
        (integer)flags_.get(flag.cache_remove_scan_count_limit));
    options.setdisabledatasync(
        (boolean)flags_.get(flag.disable_data_sync));
    options.setusefsync(
        (boolean)flags_.get(flag.use_fsync));
    options.setwaldir(
        (string)flags_.get(flag.wal_dir));
    options.setdeleteobsoletefilesperiodmicros(
        (integer)flags_.get(flag.delete_obsolete_files_period_micros));
    options.settablecachenumshardbits(
        (integer)flags_.get(flag.table_cache_numshardbits));
    options.setallowmmapreads(
        (boolean)flags_.get(flag.mmap_read));
    options.setallowmmapwrites(
        (boolean)flags_.get(flag.mmap_write));
    options.setadviserandomonopen(
        (boolean)flags_.get(flag.advise_random_on_open));
    options.setuseadaptivemutex(
        (boolean)flags_.get(flag.use_adaptive_mutex));
    options.setbytespersync(
        (long)flags_.get(flag.bytes_per_sync));
    options.setbloomlocality(
        (integer)flags_.get(flag.bloom_locality));
    options.setminwritebuffernumbertomerge(
        (integer)flags_.get(flag.min_write_buffer_number_to_merge));
    options.setmemtableprefixbloombits(
        (integer)flags_.get(flag.memtable_bloom_bits));
    options.setnumlevels(
        (integer)flags_.get(flag.num_levels));
    options.settargetfilesizebase(
        (integer)flags_.get(flag.target_file_size_base));
    options.settargetfilesizemultiplier(
        (integer)flags_.get(flag.target_file_size_multiplier));
    options.setmaxbytesforlevelbase(
        (integer)flags_.get(flag.max_bytes_for_level_base));
    options.setmaxbytesforlevelmultiplier(
        (integer)flags_.get(flag.max_bytes_for_level_multiplier));
    options.setlevelzerostopwritestrigger(
        (integer)flags_.get(flag.level0_stop_writes_trigger));
    options.setlevelzeroslowdownwritestrigger(
        (integer)flags_.get(flag.level0_slowdown_writes_trigger));
    options.setlevelzerofilenumcompactiontrigger(
        (integer)flags_.get(flag.level0_file_num_compaction_trigger));
    options.setsoftratelimit(
        (double)flags_.get(flag.soft_rate_limit));
    options.sethardratelimit(
        (double)flags_.get(flag.hard_rate_limit));
    options.setratelimitdelaymaxmilliseconds(
        (integer)flags_.get(flag.rate_limit_delay_max_milliseconds));
    options.setmaxgrandparentoverlapfactor(
        (integer)flags_.get(flag.max_grandparent_overlap_factor));
    options.setdisableautocompactions(
        (boolean)flags_.get(flag.disable_auto_compactions));
    options.setsourcecompactionfactor(
        (integer)flags_.get(flag.source_compaction_factor));
    options.setfilterdeletes(
        (boolean)flags_.get(flag.filter_deletes));
    options.setmaxsuccessivemerges(
        (integer)flags_.get(flag.max_successive_merges));
    options.setwalttlseconds((long)flags_.get(flag.wal_ttl_seconds));
    options.setwalsizelimitmb((long)flags_.get(flag.wal_size_limit_mb));
    /* todo(yhchiang): enable the following parameters
    options.setcompressiontype((string)flags_.get(flag.compression_type));
    options.setcompressionlevel((integer)flags_.get(flag.compression_level));
    options.setminleveltocompress((integer)flags_.get(flag.min_level_to_compress));
    options.sethdfs((string)flags_.get(flag.hdfs)); // env
    options.setstatistics((boolean)flags_.get(flag.statistics));
    options.setuniversalsizeratio(
        (integer)flags_.get(flag.universal_size_ratio));
    options.setuniversalminmergewidth(
        (integer)flags_.get(flag.universal_min_merge_width));
    options.setuniversalmaxmergewidth(
        (integer)flags_.get(flag.universal_max_merge_width));
    options.setuniversalmaxsizeamplificationpercent(
        (integer)flags_.get(flag.universal_max_size_amplification_percent));
    options.setuniversalcompressionsizepercent(
        (integer)flags_.get(flag.universal_compression_size_percent));
    // todo(yhchiang): add rocksdb.openforreadonly() to enable flag.readonly
    // todo(yhchiang): enable flag.merge_operator by switch
    options.setaccesshintoncompactionstart(
        (string)flags_.get(flag.compaction_fadvice));
    // available values of fadvice are "none", "normal", "sequential", "willneed" for fadvice
    */
  }

  private void run() throws rocksdbexception {
    if (!useexisting_) {
      destroydb();
    }
    options options = new options();
    prepareoptions(options);
    open(options);

    printheader(options);

    for (string benchmark : benchmarks_) {
      list<callable<stats>> tasks = new arraylist<callable<stats>>();
      list<callable<stats>> bgtasks = new arraylist<callable<stats>>();
      writeoptions writeopt = new writeoptions();
      preparewriteoptions(writeopt);
      readoptions readopt = new readoptions();
      preparereadoptions(readopt);
      int currenttaskid = 0;
      boolean known = true;

      if (benchmark.equals("fillseq")) {
        tasks.add(new writesequentialtask(
            currenttaskid++, randseed_, num_, num_, writeopt, 1));
      } else if (benchmark.equals("fillbatch")) {
        tasks.add(new writerandomtask(
            currenttaskid++, randseed_, num_ / 1000, num_, writeopt, 1000));
      } else if (benchmark.equals("fillrandom")) {
        tasks.add(new writerandomtask(
            currenttaskid++, randseed_, num_, num_, writeopt, 1));
      } else if (benchmark.equals("filluniquerandom")) {
        tasks.add(new writeuniquerandomtask(
            currenttaskid++, randseed_, num_, num_, writeopt, 1));
      } else if (benchmark.equals("fillsync")) {
        writeopt.setsync(true);
        tasks.add(new writerandomtask(
            currenttaskid++, randseed_, num_ / 1000, num_ / 1000,
            writeopt, 1));
      } else if (benchmark.equals("readseq")) {
        for (int t = 0; t < threadnum_; ++t) {
          tasks.add(new readsequentialtask(
              currenttaskid++, randseed_, reads_ / threadnum_, num_));
        }
      } else if (benchmark.equals("readrandom")) {
        for (int t = 0; t < threadnum_; ++t) {
          tasks.add(new readrandomtask(
              currenttaskid++, randseed_, reads_ / threadnum_, num_));
        }
      } else if (benchmark.equals("readwhilewriting")) {
        writetask writetask = new writerandomtask(
            -1, randseed_, long.max_value, num_, writeopt, 1, writesperseconds_);
        writetask.stats_.setexcludefrommerge();
        bgtasks.add(writetask);
        for (int t = 0; t < threadnum_; ++t) {
          tasks.add(new readrandomtask(
              currenttaskid++, randseed_, reads_ / threadnum_, num_));
        }
      } else if (benchmark.equals("readhot")) {
        for (int t = 0; t < threadnum_; ++t) {
          tasks.add(new readrandomtask(
              currenttaskid++, randseed_, reads_ / threadnum_, num_ / 100));
        }
      } else if (benchmark.equals("delete")) {
        destroydb();
        open(options);
      } else {
        known = false;
        system.err.println("unknown benchmark: " + benchmark);
      }
      if (known) {
        executorservice executor = executors.newcachedthreadpool();
        executorservice bgexecutor = executors.newcachedthreadpool();
        try {
          // measure only the main executor time
          list<future<stats>> bgresults = new arraylist<future<stats>>();
          for (callable bgtask : bgtasks) {
            bgresults.add(bgexecutor.submit(bgtask));
          }
          start();
          list<future<stats>> results = executor.invokeall(tasks);
          executor.shutdown();
          boolean finished = executor.awaittermination(10, timeunit.seconds);
          if (!finished) {
            system.out.format(
                "benchmark %s was not finished before timeout.",
                benchmark);
            executor.shutdownnow();
          }
          setfinished(true);
          bgexecutor.shutdown();
          finished = bgexecutor.awaittermination(10, timeunit.seconds);
          if (!finished) {
            system.out.format(
                "benchmark %s was not finished before timeout.",
                benchmark);
            bgexecutor.shutdownnow();
          }

          stop(benchmark, results, currenttaskid);
        } catch (interruptedexception e) {
          system.err.println(e);
        }
      }
      writeopt.dispose();
      readopt.dispose();
    }
    options.dispose();
    db_.close();
  }

  private void printheader(options options) {
    int kkeysize = 16;
    system.out.printf("keys:     %d bytes each\n", kkeysize);
    system.out.printf("values:   %d bytes each (%d bytes after compression)\n",
        valuesize_,
        (int) (valuesize_ * compressionratio_ + 0.5));
    system.out.printf("entries:  %d\n", num_);
    system.out.printf("rawsize:  %.1f mb (estimated)\n",
        ((double)(kkeysize + valuesize_) * num_) / sizeunit.mb);
    system.out.printf("filesize:   %.1f mb (estimated)\n",
        (((kkeysize + valuesize_ * compressionratio_) * num_) / sizeunit.mb));
    system.out.format("memtable factory: %s%n", options.memtablefactoryname());
    system.out.format("prefix:   %d bytes%n", prefixsize_);
    system.out.format("compression: %s%n", compressiontype_);
    printwarnings();
    system.out.printf("------------------------------------------------\n");
  }

  void printwarnings() {
    boolean assertsenabled = false;
    assert assertsenabled = true; // intentional side effect!!!
    if (assertsenabled) {
      system.out.printf(
          "warning: assertions are enabled; benchmarks unnecessarily slow\n");
    }
  }

  private void open(options options) throws rocksdbexception {
    db_ = rocksdb.open(options, databasedir_);
  }

  private void start() {
    setfinished(false);
    starttime_ = system.nanotime();
  }

  private void stop(
      string benchmark, list<future<stats>> results, int concurrentthreads) {
    long endtime = system.nanotime();
    double elapsedseconds =
        1.0d * (endtime - starttime_) / timeunit.seconds.tonanos(1);

    stats stats = new stats(-1);
    int taskfinishedcount = 0;
    for (future<stats> result : results) {
      if (result.isdone()) {
        try {
          stats taskstats = result.get(3, timeunit.seconds);
          if (!result.iscancelled()) {
            taskfinishedcount++;
          }
          stats.merge(taskstats);
        } catch (exception e) {
          // then it's not successful, the output will indicate this
        }
      }
    }
    string extra = "";
    if (benchmark.indexof("read") >= 0) {
      extra = string.format(" %d / %d found; ", stats.found_, stats.done_);
    } else {
      extra = string.format(" %d ops done; ", stats.done_);
    }

    system.out.printf(
        "%-16s : %11.5f micros/op; %6.1f mb/s;%s %d / %d task(s) finished.\n",
        benchmark, (double) elapsedseconds / stats.done_ * 1e6,
        (stats.bytes_ / 1048576.0) / elapsedseconds, extra,
        taskfinishedcount, concurrentthreads);
  }

  public void generatekeyfromlong(byte[] slice, long n) {
    assert(n >= 0);
    int startpos = 0;

    if (keysperprefix_ > 0) {
      long numprefix = (num_ + keysperprefix_ - 1) / keysperprefix_;
      long prefix = n % numprefix;
      int bytestofill = math.min(prefixsize_, 8);
      for (int i = 0; i < bytestofill; ++i) {
        slice[i] = (byte) (prefix % 256);
        prefix /= 256;
      }
      for (int i = 8; i < bytestofill; ++i) {
        slice[i] = '0';
      }
      startpos = bytestofill;
    }

    for (int i = slice.length - 1; i >= startpos; --i) {
      slice[i] = (byte) ('0' + (n % 10));
      n /= 10;
    }
  }

  private void destroydb() {
    if (db_ != null) {
      db_.close();
    }
    // todo(yhchiang): develop our own fileutil
    // fileutil.deletedir(databasedir_);
  }

  private void printstats() {
  }

  static void printhelp() {
    system.out.println("usage:");
    for (flag flag : flag.values()) {
      system.out.format("  --%s%n\t%s%n",
          flag.name(),
          flag.desc());
      if (flag.getdefaultvalue() != null) {
        system.out.format("\tdefault: %s%n",
            flag.getdefaultvalue().tostring());
      }
    }
  }

  public static void main(string[] args) throws exception {
    map<flag, object> flags = new enummap<flag, object>(flag.class);
    for (flag flag : flag.values()) {
      if (flag.getdefaultvalue() != null) {
        flags.put(flag, flag.getdefaultvalue());
      }
    }
    for (string arg : args) {
      boolean valid = false;
      if (arg.equals("--help") || arg.equals("-h")) {
        printhelp();
        system.exit(0);
      }
      if (arg.startswith("--")) {
        try {
          string[] parts = arg.substring(2).split("=");
          if (parts.length >= 1) {
            flag key = flag.valueof(parts[0]);
            if (key != null) {
              object value = null;
              if (parts.length >= 2) {
                value = key.parsevalue(parts[1]);
              }
              flags.put(key, value);
              valid = true;
            }
          }
        }
        catch (exception e) {
        }
      }
      if (!valid) {
        system.err.println("invalid argument " + arg);
        system.exit(1);
      }
    }
    new dbbenchmark(flags).run();
  }

  private enum flag {
    benchmarks(
        arrays.aslist(
            "fillseq",
            "readrandom",
            "fillrandom"),
        "comma-separated list of operations to run in the specified order\n" +
        "\tactual benchmarks:\n" +
        "\t\tfillseq          -- write n values in sequential key order in async mode.\n" +
        "\t\tfillrandom       -- write n values in random key order in async mode.\n" +
        "\t\tfillbatch        -- write n/1000 batch where each batch has 1000 values\n" +
        "\t\t                   in random key order in sync mode.\n" +
        "\t\tfillsync         -- write n/100 values in random key order in sync mode.\n" +
        "\t\tfill100k         -- write n/1000 100k values in random order in async mode.\n" +
        "\t\treadseq          -- read n times sequentially.\n" +
        "\t\treadrandom       -- read n times in random order.\n" +
        "\t\treadhot          -- read n times in random order from 1% section of db.\n" +
        "\t\treadwhilewriting -- measure the read performance of multiple readers\n" +
        "\t\t                   with a bg single writer.  the write rate of the bg\n" +
        "\t\t                   is capped by --writes_per_second.\n" +
        "\tmeta operations:\n" +
        "\t\tdelete            -- delete db") {
      @override public object parsevalue(string value) {
        return new arraylist<string>(arrays.aslist(value.split(",")));
      }
    },
    compression_ratio(0.5d,
        "arrange to generate values that shrink to this fraction of\n" +
        "\ttheir original size after compression.") {
      @override public object parsevalue(string value) {
        return double.parsedouble(value);
      }
    },
    use_existing_db(false,
        "if true, do not destroy the existing database.  if you set this\n" +
        "\tflag and also specify a benchmark that wants a fresh database,\n" +
        "\tthat benchmark will fail.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    num(1000000,
        "number of key/values to place in database.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    threads(1,
        "number of concurrent threads to run.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    reads(null,
        "number of read operations to do.  if negative, do --nums reads.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    key_size(16,
        "the size of each key in bytes.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    value_size(100,
        "the size of each value in bytes.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    write_buffer_size(4 * sizeunit.mb,
        "number of bytes to buffer in memtable before compacting\n" +
        "\t(initialized to default value by 'main'.)") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    max_write_buffer_number(2,
             "the number of in-memory memtables. each memtable is of size\n" +
             "\twrite_buffer_size.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    prefix_size(0, "controls the prefix size for hashskiplist, hashlinkedlist,\n" +
                   "\tand plain table.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    keys_per_prefix(0, "controls the average number of keys generated\n" +
             "\tper prefix, 0 means no special handling of the prefix,\n" +
             "\ti.e. use the prefix comes with the generated random number.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    memtablerep("skip_list",
        "the memtable format.  available options are\n" +
        "\tskip_list,\n" +
        "\tvector,\n" +
        "\thash_linkedlist,\n" +
        "\thash_skiplist (prefix_hash.)") {
      @override public object parsevalue(string value) {
        return value;
      }
    },
    hash_bucket_count(sizeunit.mb,
        "the number of hash buckets used in the hash-bucket-based\n" +
        "\tmemtables.  memtables that currently support this argument are\n" +
        "\thash_linkedlist and hash_skiplist.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    writes_per_second(10000,
        "the write-rate of the background writer used in the\n" +
        "\t`readwhilewriting` benchmark.  non-positive number indicates\n" +
        "\tusing an unbounded write-rate in `readwhilewriting` benchmark.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    use_plain_table(false,
        "use plain-table sst format.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    cache_size(-1l,
        "number of bytes to use as a cache of uncompressed data.\n" +
        "\tnegative means use default settings.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    seed(0l,
        "seed base for random number generators.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    num_levels(7,
        "the total number of levels.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    numdistinct(1000,
        "number of distinct keys to use. used in randomwithverify to\n" +
        "\tread/write on fewer keys so that gets are more likely to find the\n" +
        "\tkey and puts are more likely to update the same key.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    merge_keys(-1,
        "number of distinct keys to use for mergerandom and\n" +
        "\treadrandommergerandom.\n" +
        "\tif negative, there will be flags_num keys.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    bloom_locality(0,"control bloom filter probes locality.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    duration(0,"time in seconds for the random-ops tests to run.\n" +
        "\twhen 0 then num & reads determine the test duration.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    num_multi_db(0,
        "number of dbs used in the benchmark. 0 means single db.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    histogram(false,"print histogram of operation timings.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    min_write_buffer_number_to_merge(
        defaultoptions_.minwritebuffernumbertomerge(),
        "the minimum number of write buffers that will be merged together\n" +
        "\tbefore writing to storage. this is cheap because it is an\n" +
        "\tin-memory merge. if this feature is not enabled, then all these\n" +
        "\twrite buffers are flushed to l0 as separate files and this\n" +
        "\tincreases read amplification because a get request has to check\n" +
        "\tin all of these files. also, an in-memory merge may result in\n" +
        "\twriting less data to storage if there are duplicate records\n" +
        "\tin each of these individual write buffers.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    max_background_compactions(
        defaultoptions_.maxbackgroundcompactions(),
        "the maximum number of concurrent background compactions\n" +
        "\tthat can occur in parallel.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    max_background_flushes(
        defaultoptions_.maxbackgroundflushes(),
        "the maximum number of concurrent background flushes\n" +
        "\tthat can occur in parallel.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    /* todo(yhchiang): enable the following
    compaction_style((int32_t) defaultoptions_.compactionstyle(),
        "style of compaction: level-based vs universal.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },*/
    universal_size_ratio(0,
        "percentage flexibility while comparing file size\n" +
        "\t(for universal compaction only).") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    universal_min_merge_width(0,"the minimum number of files in a\n" +
        "\tsingle compaction run (for universal compaction only).") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    universal_max_merge_width(0,"the max number of files to compact\n" +
        "\tin universal style compaction.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    universal_max_size_amplification_percent(0,
        "the max size amplification for universal style compaction.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    universal_compression_size_percent(-1,
        "the percentage of the database to compress for universal\n" +
        "\tcompaction. -1 means compress everything.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    block_size(defaultblockbasedtableoptions_.blocksize(),
        "number of bytes in a block.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    compressed_cache_size(-1,
        "number of bytes to use as a cache of compressed data.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    open_files(defaultoptions_.maxopenfiles(),
        "maximum number of files to keep open at the same time\n" +
        "\t(use default if == 0)") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    bloom_bits(-1,"bloom filter bits per key. negative means\n" +
        "\tuse default settings.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    memtable_bloom_bits(0,"bloom filter bits per key for memtable.\n" +
        "\tnegative means no bloom filter.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    cache_numshardbits(-1,"number of shards for the block cache\n" +
        "\tis 2 ** cache_numshardbits. negative means use default settings.\n" +
        "\tthis is applied only if flags_cache_size is non-negative.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    cache_remove_scan_count_limit(32,"") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    verify_checksum(false,"verify checksum for every block read\n" +
        "\tfrom storage.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    statistics(false,"database statistics.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    writes(-1,"number of write operations to do. if negative, do\n" +
        "\t--num reads.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    sync(false,"sync all writes to disk.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    disable_data_sync(false,"if true, do not wait until data is\n" +
        "\tsynced to disk.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    use_fsync(false,"if true, issue fsync instead of fdatasync.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    disable_wal(false,"if true, do not write wal for write.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    wal_dir("", "if not empty, use the given dir for wal.") {
      @override public object parsevalue(string value) {
        return value;
      }
    },
    target_file_size_base(2 * 1048576,"target file size at level-1") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    target_file_size_multiplier(1,
        "a multiplier to compute target level-n file size (n >= 2)") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    max_bytes_for_level_base(10 * 1048576,
      "max bytes for level-1") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    max_bytes_for_level_multiplier(10,
        "a multiplier to compute max bytes for level-n (n >= 2)") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    level0_stop_writes_trigger(12,"number of files in level-0\n" +
        "\tthat will trigger put stop.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    level0_slowdown_writes_trigger(8,"number of files in level-0\n" +
        "\tthat will slow down writes.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    level0_file_num_compaction_trigger(4,"number of files in level-0\n" +
        "\twhen compactions start.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    readwritepercent(90,"ratio of reads to reads/writes (expressed\n" +
        "\tas percentage) for the readrandomwriterandom workload. the\n" +
        "\tdefault value 90 means 90% operations out of all reads and writes\n" +
        "\toperations are reads. in other words, 9 gets for every 1 put.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    mergereadpercent(70,"ratio of merges to merges&reads (expressed\n" +
        "\tas percentage) for the readrandommergerandom workload. the\n" +
        "\tdefault value 70 means 70% out of all read and merge operations\n" +
        "\tare merges. in other words, 7 merges for every 3 gets.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    deletepercent(2,"percentage of deletes out of reads/writes/\n" +
        "\tdeletes (used in randomwithverify only). randomwithverify\n" +
        "\tcalculates writepercent as (100 - flags_readwritepercent -\n" +
        "\tdeletepercent), so deletepercent must be smaller than (100 -\n" +
        "\tflags_readwritepercent)") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    delete_obsolete_files_period_micros(0,"option to delete\n" +
        "\tobsolete files periodically. 0 means that obsolete files are\n" +
        "\tdeleted after every compaction run.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    compression_type("snappy",
        "algorithm used to compress the database.") {
      @override public object parsevalue(string value) {
        return value;
      }
    },
    compression_level(-1,
        "compression level. for zlib this should be -1 for the\n" +
        "\tdefault level, or between 0 and 9.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    min_level_to_compress(-1,"if non-negative, compression starts\n" +
        "\tfrom this level. levels with number < min_level_to_compress are\n" +
        "\tnot compressed. otherwise, apply compression_type to\n" +
        "\tall levels.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    table_cache_numshardbits(4,"") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    stats_interval(0,"stats are reported every n operations when\n" +
        "\tthis is greater than zero. when 0 the interval grows over time.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    stats_per_interval(0,"reports additional stats per interval when\n" +
        "\tthis is greater than 0.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    perf_level(0,"level of perf collection.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    soft_rate_limit(0.0,"") {
      @override public object parsevalue(string value) {
        return double.parsedouble(value);
      }
    },
    hard_rate_limit(0.0,"when not equal to 0 this make threads\n" +
        "\tsleep at each stats reporting interval until the compaction\n" +
        "\tscore for all levels is less than or equal to this value.") {
      @override public object parsevalue(string value) {
        return double.parsedouble(value);
      }
    },
    rate_limit_delay_max_milliseconds(1000,
        "when hard_rate_limit is set then this is the max time a put will\n" +
        "\tbe stalled.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    max_grandparent_overlap_factor(10,"control maximum bytes of\n" +
        "\toverlaps in grandparent (i.e., level+2) before we stop building a\n" +
        "\tsingle file in a level->level+1 compaction.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    readonly(false,"run read only benchmarks.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    disable_auto_compactions(false,"do not auto trigger compactions.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    source_compaction_factor(1,"cap the size of data in level-k for\n" +
        "\ta compaction run that compacts level-k with level-(k+1) (for\n" +
        "\tk >= 1)") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    wal_ttl_seconds(0l,"set the ttl for the wal files in seconds.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    wal_size_limit_mb(0l,"set the size limit for the wal files\n" +
        "\tin mb.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    /* todo(yhchiang): enable the following
    bufferedio(rocksdb::envoptions().use_os_buffer,
        "allow buffered io using os buffers.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    */
    mmap_read(false,
        "allow reads to occur via mmap-ing files.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    mmap_write(false,
        "allow writes to occur via mmap-ing files.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    advise_random_on_open(defaultoptions_.adviserandomonopen(),
        "advise random access on table file open.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    compaction_fadvice("normal",
      "access pattern advice when a file is compacted.") {
      @override public object parsevalue(string value) {
        return value;
      }
    },
    use_tailing_iterator(false,
        "use tailing iterator to access a series of keys instead of get.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    use_adaptive_mutex(defaultoptions_.useadaptivemutex(),
        "use adaptive mutex.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    bytes_per_sync(defaultoptions_.bytespersync(),
        "allows os to incrementally sync files to disk while they are\n" +
        "\tbeing written, in the background. issue one request for every\n" +
        "\tbytes_per_sync written. 0 turns it off.") {
      @override public object parsevalue(string value) {
        return long.parselong(value);
      }
    },
    filter_deletes(false," on true, deletes use bloom-filter and drop\n" +
        "\tthe delete if key not present.") {
      @override public object parsevalue(string value) {
        return parseboolean(value);
      }
    },
    max_successive_merges(0,"maximum number of successive merge\n" +
        "\toperations on a key in the memtable.") {
      @override public object parsevalue(string value) {
        return integer.parseint(value);
      }
    },
    db("/tmp/rocksdbjni-bench",
       "use the db with the following name.") {
      @override public object parsevalue(string value) {
        return value;
      }
    };

    private flag(object defaultvalue, string desc) {
      defaultvalue_ = defaultvalue;
      desc_ = desc;
    }

    public object getdefaultvalue() {
      return defaultvalue_;
    }

    public string desc() {
      return desc_;
    }

    public boolean parseboolean(string value) {
      if (value.equals("1")) {
        return true;
      } else if (value.equals("0")) {
        return false;
      }
      return boolean.parseboolean(value);
    }

    protected abstract object parsevalue(string value);

    private final object defaultvalue_;
    private final string desc_;
  }

  private static class randomgenerator {
    private final byte[] data_;
    private int datalength_;
    private int position_;
    private double compressionratio_;
    random rand_;

    private randomgenerator(long seed, double compressionratio) {
      // we use a limited amount of data over and over again and ensure
      // that it is larger than the compression window (32kb), and also
      byte[] value = new byte[100];
      // large enough to serve all typical value sizes we want to write.
      rand_ = new random(seed);
      datalength_ = value.length * 10000;
      data_ = new byte[datalength_];
      compressionratio_ = compressionratio;
      int pos = 0;
      while (pos < datalength_) {
        compressiblebytes(value);
        system.arraycopy(value, 0, data_, pos,
                         math.min(value.length, datalength_ - pos));
        pos += value.length;
      }
    }

    private void compressiblebytes(byte[] value) {
      int baselength = value.length;
      if (compressionratio_ < 1.0d) {
        baselength = (int) (compressionratio_ * value.length + 0.5);
      }
      if (baselength <= 0) {
        baselength = 1;
      }
      int pos;
      for (pos = 0; pos < baselength; ++pos) {
        value[pos] = (byte) (' ' + rand_.nextint(95));  // ' ' .. '~'
      }
      while (pos < value.length) {
        system.arraycopy(value, 0, value, pos,
                         math.min(baselength, value.length - pos));
        pos += baselength;
      }
    }

    private void generate(byte[] value) {
      if (position_ + value.length > data_.length) {
        position_ = 0;
        assert(value.length <= data_.length);
      }
      position_ += value.length;
      system.arraycopy(data_, position_ - value.length,
                       value, 0, value.length);
    }
  }

  boolean isfinished() {
    synchronized(finishlock_) {
      return isfinished_;
    }
  }

  void setfinished(boolean flag) {
    synchronized(finishlock_) {
      isfinished_ = flag;
    }
  }

  rocksdb db_;
  final list<string> benchmarks_;
  final int num_;
  final int reads_;
  final int keysize_;
  final int valuesize_;
  final int threadnum_;
  final int writesperseconds_;
  final long randseed_;
  final boolean useexisting_;
  final string databasedir_;
  double compressionratio_;
  randomgenerator gen_;
  long starttime_;

  // memtable related
  final int maxwritebuffernumber_;
  final int prefixsize_;
  final int keysperprefix_;
  final string memtable_;
  final long hashbucketcount_;

  // sst format related
  boolean useplaintable_;

  object finishlock_;
  boolean isfinished_;
  map<flag, object> flags_;
  // as the scope of a static member equals to the scope of the problem,
  // we let its c++ pointer to be disposed in its finalizer.
  static options defaultoptions_ = new options();
  static blockbasedtableconfig defaultblockbasedtableoptions_ =
    new blockbasedtableconfig();
  string compressiontype_;
  compressiontype compression_;
}
