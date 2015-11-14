// protocol buffers - google's data interchange format
// copyright 2009 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

package com.google.protocolbuffers;

import java.io.bytearrayinputstream;
import java.io.bytearrayoutputstream;
import java.io.file;
import java.io.fileoutputstream;
import java.io.filenotfoundexception;
import java.io.ioexception;
import java.io.randomaccessfile;
import java.lang.reflect.method;

import com.google.protobuf.bytestring;
import com.google.protobuf.codedinputstream;
import com.google.protobuf.codedoutputstream;
import com.google.protobuf.message;

public class protobench {
  
  private static final long min_sample_time_ms = 2 * 1000;
  private static final long target_time_ms = 30 * 1000;

  private protobench() {
    // prevent instantiation
  }
  
  public static void main(string[] args) {
    if (args.length < 2 || (args.length % 2) != 0) {
      system.err.println("usage: protobench <descriptor type name> <input data>");
      system.err.println("the descriptor type name is the fully-qualified message name,");
      system.err.println("e.g. com.google.protocolbuffers.benchmark.message1");
      system.err.println("(you can specify multiple pairs of descriptor type name and input data.)");
      system.exit(1);
    }
    boolean success = true;
    for (int i = 0; i < args.length; i += 2) {
      success &= runtest(args[i], args[i + 1]);
    }
    system.exit(success ? 0 : 1);
  }

  /**
   * runs a single test. error messages are displayed to stderr, and the return value
   * indicates general success/failure.
   */
  public static boolean runtest(string type, string file) {
    system.out.println("benchmarking " + type + " with file " + file);
    final message defaultmessage;
    try {
      class<?> clazz = class.forname(type);
      method method = clazz.getdeclaredmethod("getdefaultinstance");
      defaultmessage = (message) method.invoke(null);
    } catch (exception e) {
      // we want to do the same thing with all exceptions. not generally nice,
      // but this is slightly different.
      system.err.println("unable to get default message for " + type);
      return false;
    }
    
    try {
      final byte[] inputdata = readallbytes(file);
      final bytearrayinputstream inputstream = new bytearrayinputstream(inputdata);
      final bytestring inputstring = bytestring.copyfrom(inputdata);
      final message samplemessage = defaultmessage.newbuilderfortype().mergefrom(inputstring).build();
      fileoutputstream devnulltemp = null;
      codedoutputstream reusedevnulltemp = null;
      try {
        devnulltemp = new fileoutputstream("/dev/null");
        reusedevnulltemp = codedoutputstream.newinstance(devnulltemp);
      } catch (filenotfoundexception e) {
        // ignore: this is probably windows, where /dev/null does not exist
      }
      final fileoutputstream devnull = devnulltemp;
      final codedoutputstream reusedevnull = reusedevnulltemp;
      benchmark("serialize to byte string", inputdata.length, new action() {
        public void execute() { samplemessage.tobytestring(); }
      });      
      benchmark("serialize to byte array", inputdata.length, new action() {
        public void execute() { samplemessage.tobytearray(); }
      });
      benchmark("serialize to memory stream", inputdata.length, new action() {
        public void execute() throws ioexception { 
          samplemessage.writeto(new bytearrayoutputstream()); 
        }
      });
      if (devnull != null) {
        benchmark("serialize to /dev/null with fileoutputstream", inputdata.length, new action() {
          public void execute() throws ioexception {
            samplemessage.writeto(devnull);
          }
        });
        benchmark("serialize to /dev/null reusing fileoutputstream", inputdata.length, new action() {
          public void execute() throws ioexception {
            samplemessage.writeto(reusedevnull);
            reusedevnull.flush();  // force the write to the outputstream
          }
        });
      }
      benchmark("deserialize from byte string", inputdata.length, new action() {
        public void execute() throws ioexception { 
          defaultmessage.newbuilderfortype().mergefrom(inputstring).build();
        }
      });
      benchmark("deserialize from byte array", inputdata.length, new action() {
        public void execute() throws ioexception { 
          defaultmessage.newbuilderfortype()
            .mergefrom(codedinputstream.newinstance(inputdata)).build();
        }
      });
      benchmark("deserialize from memory stream", inputdata.length, new action() {
        public void execute() throws ioexception { 
          defaultmessage.newbuilderfortype()
            .mergefrom(codedinputstream.newinstance(inputstream)).build();
          inputstream.reset();
        }
      });
      system.out.println();
      return true;
    } catch (exception e) {
      system.err.println("error: " + e.getmessage());
      system.err.println("detailed exception information:");
      e.printstacktrace(system.err);
      return false;
    }
  }
  
  private static void benchmark(string name, long datasize, action action) throws ioexception {
    // make sure it's jitted "reasonably" hard before running the first progress test
    for (int i=0; i < 100; i++) {
      action.execute();
    }
    
    // run it progressively more times until we've got a reasonable sample
    int iterations = 1;
    long elapsed = timeaction(action, iterations);
    while (elapsed < min_sample_time_ms) {
      iterations *= 2;
      elapsed = timeaction(action, iterations);
    }
    
    // upscale the sample to the target time. do this in floating point arithmetic
    // to avoid overflow issues.
    iterations = (int) ((target_time_ms / (double) elapsed) * iterations);
    elapsed = timeaction(action, iterations);
    system.out.println(name + ": " + iterations + " iterations in "
         + (elapsed/1000f) + "s; " 
         + (iterations * datasize) / (elapsed * 1024 * 1024 / 1000f) 
         + "mb/s");
  }
  
  private static long timeaction(action action, int iterations) throws ioexception {
    system.gc();    
    long start = system.currenttimemillis();
    for (int i = 0; i < iterations; i++) {
      action.execute();
    }
    long end = system.currenttimemillis();
    return end - start;
  }
  
  private static byte[] readallbytes(string filename) throws ioexception {
    randomaccessfile file = new randomaccessfile(new file(filename), "r");
    byte[] content = new byte[(int) file.length()];
    file.readfully(content);
    return content;
  }

  /**
   * interface used to capture a single action to benchmark.
   */
  interface action {
    void execute() throws ioexception;
  }
}
