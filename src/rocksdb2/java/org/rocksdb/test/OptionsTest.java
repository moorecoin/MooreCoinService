// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb.test;

import java.util.random;
import org.rocksdb.rocksdb;
import org.rocksdb.options;

public class optionstest {
  static {
    rocksdb.loadlibrary();
  }
  public static void main(string[] args) {
    options opt = new options();
    random rand = new random();
    { // createifmissing test
      boolean boolvalue = rand.nextboolean();
      opt.setcreateifmissing(boolvalue);
      assert(opt.createifmissing() == boolvalue);
    }

    { // errorifexists test
      boolean boolvalue = rand.nextboolean();
      opt.seterrorifexists(boolvalue);
      assert(opt.errorifexists() == boolvalue);
    }

    { // paranoidchecks test
      boolean boolvalue = rand.nextboolean();
      opt.setparanoidchecks(boolvalue);
      assert(opt.paranoidchecks() == boolvalue);
    }

    { // maxopenfiles test
      int intvalue = rand.nextint();
      opt.setmaxopenfiles(intvalue);
      assert(opt.maxopenfiles() == intvalue);
    }

    { // disabledatasync test
      boolean boolvalue = rand.nextboolean();
      opt.setdisabledatasync(boolvalue);
      assert(opt.disabledatasync() == boolvalue);
    }

    { // usefsync test
      boolean boolvalue = rand.nextboolean();
      opt.setusefsync(boolvalue);
      assert(opt.usefsync() == boolvalue);
    }

    { // dbstatsloginterval test
      int intvalue = rand.nextint();
      opt.setdbstatsloginterval(intvalue);
      assert(opt.dbstatsloginterval() == intvalue);
    }

    { // dblogdir test
      string str = "path/to/dblogdir";
      opt.setdblogdir(str);
      assert(opt.dblogdir().equals(str));
    }

    { // waldir test
      string str = "path/to/waldir";
      opt.setwaldir(str);
      assert(opt.waldir().equals(str));
    }

    { // deleteobsoletefilesperiodmicros test
      long longvalue = rand.nextlong();
      opt.setdeleteobsoletefilesperiodmicros(longvalue);
      assert(opt.deleteobsoletefilesperiodmicros() == longvalue);
    }

    { // maxbackgroundcompactions test
      int intvalue = rand.nextint();
      opt.setmaxbackgroundcompactions(intvalue);
      assert(opt.maxbackgroundcompactions() == intvalue);
    }

    { // maxbackgroundflushes test
      int intvalue = rand.nextint();
      opt.setmaxbackgroundflushes(intvalue);
      assert(opt.maxbackgroundflushes() == intvalue);
    }

    { // maxlogfilesize test
      long longvalue = rand.nextlong();
      opt.setmaxlogfilesize(longvalue);
      assert(opt.maxlogfilesize() == longvalue);
    }

    { // logfiletimetoroll test
      long longvalue = rand.nextlong();
      opt.setlogfiletimetoroll(longvalue);
      assert(opt.logfiletimetoroll() == longvalue);
    }

    { // keeplogfilenum test
      long longvalue = rand.nextlong();
      opt.setkeeplogfilenum(longvalue);
      assert(opt.keeplogfilenum() == longvalue);
    }

    { // maxmanifestfilesize test
      long longvalue = rand.nextlong();
      opt.setmaxmanifestfilesize(longvalue);
      assert(opt.maxmanifestfilesize() == longvalue);
    }

    { // tablecachenumshardbits test
      int intvalue = rand.nextint();
      opt.settablecachenumshardbits(intvalue);
      assert(opt.tablecachenumshardbits() == intvalue);
    }

    { // tablecacheremovescancountlimit test
      int intvalue = rand.nextint();
      opt.settablecacheremovescancountlimit(intvalue);
      assert(opt.tablecacheremovescancountlimit() == intvalue);
    }

    { // walttlseconds test
      long longvalue = rand.nextlong();
      opt.setwalttlseconds(longvalue);
      assert(opt.walttlseconds() == longvalue);
    }

    { // manifestpreallocationsize test
      long longvalue = rand.nextlong();
      opt.setmanifestpreallocationsize(longvalue);
      assert(opt.manifestpreallocationsize() == longvalue);
    }

    { // allowosbuffer test
      boolean boolvalue = rand.nextboolean();
      opt.setallowosbuffer(boolvalue);
      assert(opt.allowosbuffer() == boolvalue);
    }

    { // allowmmapreads test
      boolean boolvalue = rand.nextboolean();
      opt.setallowmmapreads(boolvalue);
      assert(opt.allowmmapreads() == boolvalue);
    }

    { // allowmmapwrites test
      boolean boolvalue = rand.nextboolean();
      opt.setallowmmapwrites(boolvalue);
      assert(opt.allowmmapwrites() == boolvalue);
    }

    { // isfdcloseonexec test
      boolean boolvalue = rand.nextboolean();
      opt.setisfdcloseonexec(boolvalue);
      assert(opt.isfdcloseonexec() == boolvalue);
    }

    { // skiplogerroronrecovery test
      boolean boolvalue = rand.nextboolean();
      opt.setskiplogerroronrecovery(boolvalue);
      assert(opt.skiplogerroronrecovery() == boolvalue);
    }

    { // statsdumpperiodsec test
      int intvalue = rand.nextint();
      opt.setstatsdumpperiodsec(intvalue);
      assert(opt.statsdumpperiodsec() == intvalue);
    }

    { // adviserandomonopen test
      boolean boolvalue = rand.nextboolean();
      opt.setadviserandomonopen(boolvalue);
      assert(opt.adviserandomonopen() == boolvalue);
    }

    { // useadaptivemutex test
      boolean boolvalue = rand.nextboolean();
      opt.setuseadaptivemutex(boolvalue);
      assert(opt.useadaptivemutex() == boolvalue);
    }

    { // bytespersync test
      long longvalue = rand.nextlong();
      opt.setbytespersync(longvalue);
      assert(opt.bytespersync() == longvalue);
    }

    { // allowthreadlocal test
      boolean boolvalue = rand.nextboolean();
      opt.setallowthreadlocal(boolvalue);
      assert(opt.allowthreadlocal() == boolvalue);
    }

    { // writebuffersize test
      long longvalue = rand.nextlong();
      opt.setwritebuffersize(longvalue);
      assert(opt.writebuffersize() == longvalue);
    }

    { // maxwritebuffernumber test
      int intvalue = rand.nextint();
      opt.setmaxwritebuffernumber(intvalue);
      assert(opt.maxwritebuffernumber() == intvalue);
    }

    { // minwritebuffernumbertomerge test
      int intvalue = rand.nextint();
      opt.setminwritebuffernumbertomerge(intvalue);
      assert(opt.minwritebuffernumbertomerge() == intvalue);
    }

    { // numlevels test
      int intvalue = rand.nextint();
      opt.setnumlevels(intvalue);
      assert(opt.numlevels() == intvalue);
    }

    { // levelfilenumcompactiontrigger test
      int intvalue = rand.nextint();
      opt.setlevelzerofilenumcompactiontrigger(intvalue);
      assert(opt.levelzerofilenumcompactiontrigger() == intvalue);
    }

    { // levelslowdownwritestrigger test
      int intvalue = rand.nextint();
      opt.setlevelzeroslowdownwritestrigger(intvalue);
      assert(opt.levelzeroslowdownwritestrigger() == intvalue);
    }

    { // levelstopwritestrigger test
      int intvalue = rand.nextint();
      opt.setlevelzerostopwritestrigger(intvalue);
      assert(opt.levelzerostopwritestrigger() == intvalue);
    }

    { // maxmemcompactionlevel test
      int intvalue = rand.nextint();
      opt.setmaxmemcompactionlevel(intvalue);
      assert(opt.maxmemcompactionlevel() == intvalue);
    }

    { // targetfilesizebase test
      int intvalue = rand.nextint();
      opt.settargetfilesizebase(intvalue);
      assert(opt.targetfilesizebase() == intvalue);
    }

    { // targetfilesizemultiplier test
      int intvalue = rand.nextint();
      opt.settargetfilesizemultiplier(intvalue);
      assert(opt.targetfilesizemultiplier() == intvalue);
    }

    { // maxbytesforlevelbase test
      long longvalue = rand.nextlong();
      opt.setmaxbytesforlevelbase(longvalue);
      assert(opt.maxbytesforlevelbase() == longvalue);
    }

    { // maxbytesforlevelmultiplier test
      int intvalue = rand.nextint();
      opt.setmaxbytesforlevelmultiplier(intvalue);
      assert(opt.maxbytesforlevelmultiplier() == intvalue);
    }

    { // expandedcompactionfactor test
      int intvalue = rand.nextint();
      opt.setexpandedcompactionfactor(intvalue);
      assert(opt.expandedcompactionfactor() == intvalue);
    }

    { // sourcecompactionfactor test
      int intvalue = rand.nextint();
      opt.setsourcecompactionfactor(intvalue);
      assert(opt.sourcecompactionfactor() == intvalue);
    }

    { // maxgrandparentoverlapfactor test
      int intvalue = rand.nextint();
      opt.setmaxgrandparentoverlapfactor(intvalue);
      assert(opt.maxgrandparentoverlapfactor() == intvalue);
    }

    { // softratelimit test
      double doublevalue = rand.nextdouble();
      opt.setsoftratelimit(doublevalue);
      assert(opt.softratelimit() == doublevalue);
    }

    { // hardratelimit test
      double doublevalue = rand.nextdouble();
      opt.sethardratelimit(doublevalue);
      assert(opt.hardratelimit() == doublevalue);
    }

    { // ratelimitdelaymaxmilliseconds test
      int intvalue = rand.nextint();
      opt.setratelimitdelaymaxmilliseconds(intvalue);
      assert(opt.ratelimitdelaymaxmilliseconds() == intvalue);
    }

    { // arenablocksize test
      long longvalue = rand.nextlong();
      opt.setarenablocksize(longvalue);
      assert(opt.arenablocksize() == longvalue);
    }

    { // disableautocompactions test
      boolean boolvalue = rand.nextboolean();
      opt.setdisableautocompactions(boolvalue);
      assert(opt.disableautocompactions() == boolvalue);
    }

    { // purgeredundantkvswhileflush test
      boolean boolvalue = rand.nextboolean();
      opt.setpurgeredundantkvswhileflush(boolvalue);
      assert(opt.purgeredundantkvswhileflush() == boolvalue);
    }

    { // verifychecksumsincompaction test
      boolean boolvalue = rand.nextboolean();
      opt.setverifychecksumsincompaction(boolvalue);
      assert(opt.verifychecksumsincompaction() == boolvalue);
    }

    { // filterdeletes test
      boolean boolvalue = rand.nextboolean();
      opt.setfilterdeletes(boolvalue);
      assert(opt.filterdeletes() == boolvalue);
    }

    { // maxsequentialskipiniterations test
      long longvalue = rand.nextlong();
      opt.setmaxsequentialskipiniterations(longvalue);
      assert(opt.maxsequentialskipiniterations() == longvalue);
    }

    { // inplaceupdatesupport test
      boolean boolvalue = rand.nextboolean();
      opt.setinplaceupdatesupport(boolvalue);
      assert(opt.inplaceupdatesupport() == boolvalue);
    }

    { // inplaceupdatenumlocks test
      long longvalue = rand.nextlong();
      opt.setinplaceupdatenumlocks(longvalue);
      assert(opt.inplaceupdatenumlocks() == longvalue);
    }

    { // memtableprefixbloombits test
      int intvalue = rand.nextint();
      opt.setmemtableprefixbloombits(intvalue);
      assert(opt.memtableprefixbloombits() == intvalue);
    }

    { // memtableprefixbloomprobes test
      int intvalue = rand.nextint();
      opt.setmemtableprefixbloomprobes(intvalue);
      assert(opt.memtableprefixbloomprobes() == intvalue);
    }

    { // bloomlocality test
      int intvalue = rand.nextint();
      opt.setbloomlocality(intvalue);
      assert(opt.bloomlocality() == intvalue);
    }

    { // maxsuccessivemerges test
      long longvalue = rand.nextlong();
      opt.setmaxsuccessivemerges(longvalue);
      assert(opt.maxsuccessivemerges() == longvalue);
    }

    { // minpartialmergeoperands test
      int intvalue = rand.nextint();
      opt.setminpartialmergeoperands(intvalue);
      assert(opt.minpartialmergeoperands() == intvalue);
    }

    opt.dispose();
    system.out.println("passed optionstest");
  }
}
