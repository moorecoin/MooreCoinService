import os
import os.path
import shutil
import subprocess
import time
import unittest
import tempfile

def my_check_output(*popenargs, **kwargs):
    """
    if we had python 2.7, we should simply use subprocess.check_output.
    this is a stop-gap solution for python 2.6
    """
    if 'stdout' in kwargs:
        raise valueerror('stdout argument not allowed, it will be overridden.')
    process = subprocess.popen(stderr=subprocess.pipe, stdout=subprocess.pipe,
                               *popenargs, **kwargs)
    output, unused_err = process.communicate()
    retcode = process.poll()
    if retcode:
        cmd = kwargs.get("args")
        if cmd is none:
            cmd = popenargs[0]
        raise exception("exit code is not 0.  it is %d.  command: %s" %
                (retcode, cmd))
    return output

def run_err_null(cmd):
    return os.system(cmd + " 2>/dev/null ")

class ldbtestcase(unittest.testcase):
    def setup(self):
        self.tmp_dir  = tempfile.mkdtemp(prefix="ldb_test_")
        self.db_name = "testdb"

    def teardown(self):
        assert(self.tmp_dir.strip() != "/"
                and self.tmp_dir.strip() != "/tmp"
                and self.tmp_dir.strip() != "/tmp/") #just some paranoia

        shutil.rmtree(self.tmp_dir)

    def dbparam(self, dbname):
        return "--db=%s" % os.path.join(self.tmp_dir, dbname)

    def assertrunokfull(self, params, expectedoutput, unexpected=false):
        """
        all command-line params must be specified.
        allows full flexibility in testing; for example: missing db param.

        """

        output = my_check_output("./ldb %s |grep -v \"created bg thread\"" %
                            params, shell=true)
        if not unexpected:
            self.assertequal(output.strip(), expectedoutput.strip())
        else:
            self.assertnotequal(output.strip(), expectedoutput.strip())

    def assertrunfailfull(self, params):
        """
        all command-line params must be specified.
        allows full flexibility in testing; for example: missing db param.

        """
        try:

            my_check_output("./ldb %s >/dev/null 2>&1 |grep -v \"created bg \
                thread\"" % params, shell=true)
        except exception, e:
            return
        self.fail(
            "exception should have been raised for command with params: %s" %
            params)

    def assertrunok(self, params, expectedoutput, unexpected=false):
        """
        uses the default test db.

        """
        self.assertrunokfull("%s %s" % (self.dbparam(self.db_name), params),
                             expectedoutput, unexpected)

    def assertrunfail(self, params):
        """
        uses the default test db.
        """
        self.assertrunfailfull("%s %s" % (self.dbparam(self.db_name), params))

    def testsimplestringputget(self):
        print "running testsimplestringputget..."
        self.assertrunfail("put x1 y1")
        self.assertrunok("put --create_if_missing x1 y1", "ok")
        self.assertrunok("get x1", "y1")
        self.assertrunfail("get x2")

        self.assertrunok("put x2 y2", "ok")
        self.assertrunok("get x1", "y1")
        self.assertrunok("get x2", "y2")
        self.assertrunfail("get x3")

        self.assertrunok("scan --from=x1 --to=z", "x1 : y1\nx2 : y2")
        self.assertrunok("put x3 y3", "ok")

        self.assertrunok("scan --from=x1 --to=z", "x1 : y1\nx2 : y2\nx3 : y3")
        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3")
        self.assertrunok("scan --from=x", "x1 : y1\nx2 : y2\nx3 : y3")

        self.assertrunok("scan --to=x2", "x1 : y1")
        self.assertrunok("scan --from=x1 --to=z --max_keys=1", "x1 : y1")
        self.assertrunok("scan --from=x1 --to=z --max_keys=2",
                "x1 : y1\nx2 : y2")

        self.assertrunok("scan --from=x1 --to=z --max_keys=3",
                "x1 : y1\nx2 : y2\nx3 : y3")
        self.assertrunok("scan --from=x1 --to=z --max_keys=4",
                "x1 : y1\nx2 : y2\nx3 : y3")
        self.assertrunok("scan --from=x1 --to=x2", "x1 : y1")
        self.assertrunok("scan --from=x2 --to=x4", "x2 : y2\nx3 : y3")
        self.assertrunfail("scan --from=x4 --to=z") # no results => fail
        self.assertrunfail("scan --from=x1 --to=z --max_keys=foo")

        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3")

        self.assertrunok("delete x1", "ok")
        self.assertrunok("scan", "x2 : y2\nx3 : y3")

        self.assertrunok("delete nonexistentkey", "ok")
        # it is weird that get and scan raise exception for
        # non-existent key, while delete does not

        self.assertrunok("checkconsistency", "ok")

    def dumpdb(self, params, dumpfile):
        return 0 == run_err_null("./ldb dump %s > %s" % (params, dumpfile))

    def loaddb(self, params, dumpfile):
        return 0 == run_err_null("cat %s | ./ldb load %s" % (dumpfile, params))

    def teststringbatchput(self):
        print "running teststringbatchput..."
        self.assertrunok("batchput x1 y1 --create_if_missing", "ok")
        self.assertrunok("scan", "x1 : y1")
        self.assertrunok("batchput x2 y2 x3 y3 \"x4 abc\" \"y4 xyz\"", "ok")
        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3\nx4 abc : y4 xyz")
        self.assertrunfail("batchput")
        self.assertrunfail("batchput k1")
        self.assertrunfail("batchput k1 v1 k2")

    def testcountdelimdump(self):
        print "running testcountdelimdump..."
        self.assertrunok("batchput x.1 x1 --create_if_missing", "ok")
        self.assertrunok("batchput y.abc abc y.2 2 z.13c pqr", "ok")
        self.assertrunok("dump --count_delim", "x => count:1\tsize:5\ny => count:2\tsize:12\nz => count:1\tsize:8")
        self.assertrunok("dump --count_delim=\".\"", "x => count:1\tsize:5\ny => count:2\tsize:12\nz => count:1\tsize:8")
        self.assertrunok("batchput x,2 x2 x,abc xabc", "ok")
        self.assertrunok("dump --count_delim=\",\"", "x => count:2\tsize:14\nx.1 => count:1\tsize:5\ny.2 => count:1\tsize:4\ny.abc => count:1\tsize:8\nz.13c => count:1\tsize:8")

    def testcountdelimidump(self):
        print "running testcountdelimidump..."
        self.assertrunok("batchput x.1 x1 --create_if_missing", "ok")
        self.assertrunok("batchput y.abc abc y.2 2 z.13c pqr", "ok")
        self.assertrunok("dump --count_delim", "x => count:1\tsize:5\ny => count:2\tsize:12\nz => count:1\tsize:8")
        self.assertrunok("dump --count_delim=\".\"", "x => count:1\tsize:5\ny => count:2\tsize:12\nz => count:1\tsize:8")
        self.assertrunok("batchput x,2 x2 x,abc xabc", "ok")
        self.assertrunok("dump --count_delim=\",\"", "x => count:2\tsize:14\nx.1 => count:1\tsize:5\ny.2 => count:1\tsize:4\ny.abc => count:1\tsize:8\nz.13c => count:1\tsize:8")

    def testinvalidcmdlines(self):
        print "running testinvalidcmdlines..."
        # db not specified
        self.assertrunfailfull("put 0x6133 0x6233 --hex --create_if_missing")
        # no param called he
        self.assertrunfail("put 0x6133 0x6233 --he --create_if_missing")
        # max_keys is not applicable for put
        self.assertrunfail("put 0x6133 0x6233 --max_keys=1 --create_if_missing")
        # hex has invalid boolean value

    def testhexputget(self):
        print "running testhexputget..."
        self.assertrunok("put a1 b1 --create_if_missing", "ok")
        self.assertrunok("scan", "a1 : b1")
        self.assertrunok("scan --hex", "0x6131 : 0x6231")
        self.assertrunfail("put --hex 6132 6232")
        self.assertrunok("put --hex 0x6132 0x6232", "ok")
        self.assertrunok("scan --hex", "0x6131 : 0x6231\n0x6132 : 0x6232")
        self.assertrunok("scan", "a1 : b1\na2 : b2")
        self.assertrunok("get a1", "b1")
        self.assertrunok("get --hex 0x6131", "0x6231")
        self.assertrunok("get a2", "b2")
        self.assertrunok("get --hex 0x6132", "0x6232")
        self.assertrunok("get --key_hex 0x6132", "b2")
        self.assertrunok("get --key_hex --value_hex 0x6132", "0x6232")
        self.assertrunok("get --value_hex a2", "0x6232")
        self.assertrunok("scan --key_hex --value_hex",
                "0x6131 : 0x6231\n0x6132 : 0x6232")
        self.assertrunok("scan --hex --from=0x6131 --to=0x6133",
                "0x6131 : 0x6231\n0x6132 : 0x6232")
        self.assertrunok("scan --hex --from=0x6131 --to=0x6132",
                "0x6131 : 0x6231")
        self.assertrunok("scan --key_hex", "0x6131 : b1\n0x6132 : b2")
        self.assertrunok("scan --value_hex", "a1 : 0x6231\na2 : 0x6232")
        self.assertrunok("batchput --hex 0x6133 0x6233 0x6134 0x6234", "ok")
        self.assertrunok("scan", "a1 : b1\na2 : b2\na3 : b3\na4 : b4")
        self.assertrunok("delete --hex 0x6133", "ok")
        self.assertrunok("scan", "a1 : b1\na2 : b2\na4 : b4")
        self.assertrunok("checkconsistency", "ok")

    def testttlputget(self):
        print "running testttlputget..."
        self.assertrunok("put a1 b1 --ttl --create_if_missing", "ok")
        self.assertrunok("scan --hex", "0x6131 : 0x6231", true)
        self.assertrunok("dump --ttl ", "a1 ==> b1", true)
        self.assertrunok("dump --hex --ttl ",
                         "0x6131 ==> 0x6231\nkeys in range: 1")
        self.assertrunok("scan --hex --ttl", "0x6131 : 0x6231")
        self.assertrunok("get --value_hex a1", "0x6231", true)
        self.assertrunok("get --ttl a1", "b1")
        self.assertrunok("put a3 b3 --create_if_missing", "ok")
        # fails because timstamp's length is greater than value's
        self.assertrunfail("get --ttl a3")
        self.assertrunok("checkconsistency", "ok")

    def testinvalidcmdlines(self):
        print "running testinvalidcmdlines..."
        # db not specified
        self.assertrunfailfull("put 0x6133 0x6233 --hex --create_if_missing")
        # no param called he
        self.assertrunfail("put 0x6133 0x6233 --he --create_if_missing")
        # max_keys is not applicable for put
        self.assertrunfail("put 0x6133 0x6233 --max_keys=1 --create_if_missing")
        # hex has invalid boolean value
        self.assertrunfail("put 0x6133 0x6233 --hex=boo --create_if_missing")

    def testdumpload(self):
        print "running testdumpload..."
        self.assertrunok("batchput --create_if_missing x1 y1 x2 y2 x3 y3 x4 y4",
                "ok")
        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")
        origdbpath = os.path.join(self.tmp_dir, self.db_name)

        # dump and load without any additional params specified
        dumpfilepath = os.path.join(self.tmp_dir, "dump1")
        loadeddbpath = os.path.join(self.tmp_dir, "loaded_from_dump1")
        self.asserttrue(self.dumpdb("--db=%s" % origdbpath, dumpfilepath))
        self.asserttrue(self.loaddb(
            "--db=%s --create_if_missing" % loadeddbpath, dumpfilepath))
        self.assertrunokfull("scan --db=%s" % loadeddbpath,
                "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

        # dump and load in hex
        dumpfilepath = os.path.join(self.tmp_dir, "dump2")
        loadeddbpath = os.path.join(self.tmp_dir, "loaded_from_dump2")
        self.asserttrue(self.dumpdb("--db=%s --hex" % origdbpath, dumpfilepath))
        self.asserttrue(self.loaddb(
            "--db=%s --hex --create_if_missing" % loadeddbpath, dumpfilepath))
        self.assertrunokfull("scan --db=%s" % loadeddbpath,
                "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

        # dump only a portion of the key range
        dumpfilepath = os.path.join(self.tmp_dir, "dump3")
        loadeddbpath = os.path.join(self.tmp_dir, "loaded_from_dump3")
        self.asserttrue(self.dumpdb(
            "--db=%s --from=x1 --to=x3" % origdbpath, dumpfilepath))
        self.asserttrue(self.loaddb(
            "--db=%s --create_if_missing" % loadeddbpath, dumpfilepath))
        self.assertrunokfull("scan --db=%s" % loadeddbpath, "x1 : y1\nx2 : y2")

        # dump upto max_keys rows
        dumpfilepath = os.path.join(self.tmp_dir, "dump4")
        loadeddbpath = os.path.join(self.tmp_dir, "loaded_from_dump4")
        self.asserttrue(self.dumpdb(
            "--db=%s --max_keys=3" % origdbpath, dumpfilepath))
        self.asserttrue(self.loaddb(
            "--db=%s --create_if_missing" % loadeddbpath, dumpfilepath))
        self.assertrunokfull("scan --db=%s" % loadeddbpath,
                "x1 : y1\nx2 : y2\nx3 : y3")

        # load into an existing db, create_if_missing is not specified
        self.asserttrue(self.dumpdb("--db=%s" % origdbpath, dumpfilepath))
        self.asserttrue(self.loaddb("--db=%s" % loadeddbpath, dumpfilepath))
        self.assertrunokfull("scan --db=%s" % loadeddbpath,
                "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

        # dump and load with wal disabled
        dumpfilepath = os.path.join(self.tmp_dir, "dump5")
        loadeddbpath = os.path.join(self.tmp_dir, "loaded_from_dump5")
        self.asserttrue(self.dumpdb("--db=%s" % origdbpath, dumpfilepath))
        self.asserttrue(self.loaddb(
            "--db=%s --disable_wal --create_if_missing" % loadeddbpath,
            dumpfilepath))
        self.assertrunokfull("scan --db=%s" % loadeddbpath,
                "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

        # dump and load with lots of extra params specified
        extraparams = " ".join(["--bloom_bits=14", "--compression_type=bzip2",
                                "--block_size=1024", "--auto_compaction=true",
                                "--write_buffer_size=4194304",
                                "--file_size=2097152"])
        dumpfilepath = os.path.join(self.tmp_dir, "dump6")
        loadeddbpath = os.path.join(self.tmp_dir, "loaded_from_dump6")
        self.asserttrue(self.dumpdb(
            "--db=%s %s" % (origdbpath, extraparams), dumpfilepath))
        self.asserttrue(self.loaddb(
            "--db=%s %s --create_if_missing" % (loadeddbpath, extraparams),
            dumpfilepath))
        self.assertrunokfull("scan --db=%s" % loadeddbpath,
                "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

        # dump with count_only
        dumpfilepath = os.path.join(self.tmp_dir, "dump7")
        loadeddbpath = os.path.join(self.tmp_dir, "loaded_from_dump7")
        self.asserttrue(self.dumpdb(
            "--db=%s --count_only" % origdbpath, dumpfilepath))
        self.asserttrue(self.loaddb(
            "--db=%s --create_if_missing" % loadeddbpath, dumpfilepath))
        # db should have atleast one value for scan to work
        self.assertrunokfull("put --db=%s k1 v1" % loadeddbpath, "ok")
        self.assertrunokfull("scan --db=%s" % loadeddbpath, "k1 : v1")

        # dump command fails because of typo in params
        dumpfilepath = os.path.join(self.tmp_dir, "dump8")
        self.assertfalse(self.dumpdb(
            "--db=%s --create_if_missing" % origdbpath, dumpfilepath))

    def testmiscadmintask(self):
        print "running testmiscadmintask..."
        # these tests need to be improved; for example with asserts about
        # whether compaction or level reduction actually took place.
        self.assertrunok("batchput --create_if_missing x1 y1 x2 y2 x3 y3 x4 y4",
                "ok")
        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")
        origdbpath = os.path.join(self.tmp_dir, self.db_name)

        self.asserttrue(0 == run_err_null(
            "./ldb compact --db=%s" % origdbpath))
        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

        self.asserttrue(0 == run_err_null(
            "./ldb reduce_levels --db=%s --new_levels=2" % origdbpath))
        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

        self.asserttrue(0 == run_err_null(
            "./ldb reduce_levels --db=%s --new_levels=3" % origdbpath))
        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

        self.asserttrue(0 == run_err_null(
            "./ldb compact --db=%s --from=x1 --to=x3" % origdbpath))
        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

        self.asserttrue(0 == run_err_null(
            "./ldb compact --db=%s --hex --from=0x6131 --to=0x6134"
            % origdbpath))
        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

        #todo(dilip): not sure what should be passed to wal.currently corrupted.
        self.asserttrue(0 == run_err_null(
            "./ldb dump_wal --db=%s --walfile=%s --header" % (
                origdbpath, os.path.join(origdbpath, "log"))))
        self.assertrunok("scan", "x1 : y1\nx2 : y2\nx3 : y3\nx4 : y4")

    def testcheckconsistency(self):
        print "running testcheckconsistency..."

        dbpath = os.path.join(self.tmp_dir, self.db_name)
        self.assertrunok("put x1 y1 --create_if_missing", "ok")
        self.assertrunok("put x2 y2", "ok")
        self.assertrunok("get x1", "y1")
        self.assertrunok("checkconsistency", "ok")

        sstfilepath = my_check_output("ls %s" % os.path.join(dbpath, "*.sst"),
                                      shell=true)

        # modify the file
        my_check_output("echo 'evil' > %s" % sstfilepath, shell=true)
        self.assertrunfail("checkconsistency")

        # delete the file
        my_check_output("rm -f %s" % sstfilepath, shell=true)
        self.assertrunfail("checkconsistency")


if __name__ == "__main__":
    unittest.main()
