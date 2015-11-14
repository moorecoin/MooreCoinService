// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
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

// author: kenton@google.com (kenton varda)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.
//
// implements the protocol compiler front-end such that it may be reused by
// custom compilers written to support other languages.

#ifndef google_protobuf_compiler_command_line_interface_h__
#define google_protobuf_compiler_command_line_interface_h__

#include <google/protobuf/stubs/common.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>

namespace google {
namespace protobuf {

class filedescriptor;        // descriptor.h
class descriptorpool;        // descriptor.h
class filedescriptorproto;   // descriptor.pb.h
template<typename t> class repeatedptrfield;  // repeated_field.h

namespace compiler {

class codegenerator;        // code_generator.h
class generatorcontext;      // code_generator.h
class disksourcetree;       // importer.h

// this class implements the command-line interface to the protocol compiler.
// it is designed to make it very easy to create a custom protocol compiler
// supporting the languages of your choice.  for example, if you wanted to
// create a custom protocol compiler binary which includes both the regular
// c++ support plus support for your own custom output "foo", you would
// write a class "foogenerator" which implements the codegenerator interface,
// then write a main() procedure like this:
//
//   int main(int argc, char* argv[]) {
//     google::protobuf::compiler::commandlineinterface cli;
//
//     // support generation of c++ source and headers.
//     google::protobuf::compiler::cpp::cppgenerator cpp_generator;
//     cli.registergenerator("--cpp_out", &cpp_generator,
//       "generate c++ source and header.");
//
//     // support generation of foo code.
//     foogenerator foo_generator;
//     cli.registergenerator("--foo_out", &foo_generator,
//       "generate foo file.");
//
//     return cli.run(argc, argv);
//   }
//
// the compiler is invoked with syntax like:
//   protoc --cpp_out=outdir --foo_out=outdir --proto_path=src src/foo.proto
//
// for a full description of the command-line syntax, invoke it with --help.
class libprotoc_export commandlineinterface {
 public:
  commandlineinterface();
  ~commandlineinterface();

  // register a code generator for a language.
  //
  // parameters:
  // * flag_name: the command-line flag used to specify an output file of
  //   this type.  the name must start with a '-'.  if the name is longer
  //   than one letter, it must start with two '-'s.
  // * generator: the codegenerator which will be called to generate files
  //   of this type.
  // * help_text: text describing this flag in the --help output.
  //
  // some generators accept extra parameters.  you can specify this parameter
  // on the command-line by placing it before the output directory, separated
  // by a colon:
  //   protoc --foo_out=enable_bar:outdir
  // the text before the colon is passed to codegenerator::generate() as the
  // "parameter".
  void registergenerator(const string& flag_name,
                         codegenerator* generator,
                         const string& help_text);

  // register a code generator for a language.
  // besides flag_name you can specify another option_flag_name that could be
  // used to pass extra parameters to the registered code generator.
  // suppose you have registered a generator by calling:
  //   command_line_interface.registergenerator("--foo_out", "--foo_opt", ...)
  // then you could invoke the compiler with a command like:
  //   protoc --foo_out=enable_bar:outdir --foo_opt=enable_baz
  // this will pass "enable_bar,enable_baz" as the parameter to the generator.
  void registergenerator(const string& flag_name,
                         const string& option_flag_name,
                         codegenerator* generator,
                         const string& help_text);

  // enables "plugins".  in this mode, if a command-line flag ends with "_out"
  // but does not match any registered generator, the compiler will attempt to
  // find a "plugin" to implement the generator.  plugins are just executables.
  // they should live somewhere in the path.
  //
  // the compiler determines the executable name to search for by concatenating
  // exe_name_prefix with the unrecognized flag name, removing "_out".  so, for
  // example, if exe_name_prefix is "protoc-" and you pass the flag --foo_out,
  // the compiler will try to run the program "protoc-foo".
  //
  // the plugin program should implement the following usage:
  //   plugin [--out=outdir] [--parameter=parameter] proto_files < descriptors
  // --out indicates the output directory (as passed to the --foo_out
  // parameter); if omitted, the current directory should be used.  --parameter
  // gives the generator parameter, if any was provided.  the proto_files list
  // the .proto files which were given on the compiler command-line; these are
  // the files for which the plugin is expected to generate output code.
  // finally, descriptors is an encoded filedescriptorset (as defined in
  // descriptor.proto).  this is piped to the plugin's stdin.  the set will
  // include descriptors for all the files listed in proto_files as well as
  // all files that they import.  the plugin must not attempt to read the
  // proto_files directly -- it must use the filedescriptorset.
  //
  // the plugin should generate whatever files are necessary, as code generators
  // normally do.  it should write the names of all files it generates to
  // stdout.  the names should be relative to the output directory, not absolute
  // names or relative to the current directory.  if any errors occur, error
  // messages should be written to stderr.  if an error is fatal, the plugin
  // should exit with a non-zero exit code.
  void allowplugins(const string& exe_name_prefix);

  // run the protocol compiler with the given command-line parameters.
  // returns the error code which should be returned by main().
  //
  // it may not be safe to call run() in a multi-threaded environment because
  // it calls strerror().  i'm not sure why you'd want to do this anyway.
  int run(int argc, const char* const argv[]);

  // call setinputsarecwdrelative(true) if the input files given on the command
  // line should be interpreted relative to the proto import path specified
  // using --proto_path or -i flags.  otherwise, input file names will be
  // interpreted relative to the current working directory (or as absolute
  // paths if they start with '/'), though they must still reside inside
  // a directory given by --proto_path or the compiler will fail.  the latter
  // mode is generally more intuitive and easier to use, especially e.g. when
  // defining implicit rules in makefiles.
  void setinputsareprotopathrelative(bool enable) {
    inputs_are_proto_path_relative_ = enable;
  }

  // provides some text which will be printed when the --version flag is
  // used.  the version of libprotoc will also be printed on the next line
  // after this text.
  void setversioninfo(const string& text) {
    version_info_ = text;
  }


 private:
  // -----------------------------------------------------------------

  class errorprinter;
  class generatorcontextimpl;
  class memoryoutputstream;

  // clear state from previous run().
  void clear();

  // remaps each file in input_files_ so that it is relative to one of the
  // directories in proto_path_.  returns false if an error occurred.  this
  // is only used if inputs_are_proto_path_relative_ is false.
  bool makeinputsbeprotopathrelative(
    disksourcetree* source_tree);

  // return status for parsearguments() and interpretargument().
  enum parseargumentstatus {
    parse_argument_done_and_continue,
    parse_argument_done_and_exit,
    parse_argument_fail
  };

  // parse all command-line arguments.
  parseargumentstatus parsearguments(int argc, const char* const argv[]);

  // parses a command-line argument into a name/value pair.  returns
  // true if the next argument in the argv should be used as the value,
  // false otherwise.
  //
  // exmaples:
  //   "-isrc/protos" ->
  //     name = "-i", value = "src/protos"
  //   "--cpp_out=src/foo.pb2.cc" ->
  //     name = "--cpp_out", value = "src/foo.pb2.cc"
  //   "foo.proto" ->
  //     name = "", value = "foo.proto"
  bool parseargument(const char* arg, string* name, string* value);

  // interprets arguments parsed with parseargument.
  parseargumentstatus interpretargument(const string& name,
                                        const string& value);

  // print the --help text to stderr.
  void printhelptext();

  // generate the given output file from the given input.
  struct outputdirective;  // see below
  bool generateoutput(const vector<const filedescriptor*>& parsed_files,
                      const outputdirective& output_directive,
                      generatorcontext* generator_context);
  bool generatepluginoutput(const vector<const filedescriptor*>& parsed_files,
                            const string& plugin_name,
                            const string& parameter,
                            generatorcontext* generator_context,
                            string* error);

  // implements --encode and --decode.
  bool encodeordecode(const descriptorpool* pool);

  // implements the --descriptor_set_out option.
  bool writedescriptorset(const vector<const filedescriptor*> parsed_files);

  // get all transitive dependencies of the given file (including the file
  // itself), adding them to the given list of filedescriptorprotos.  the
  // protos will be ordered such that every file is listed before any file that
  // depends on it, so that you can call descriptorpool::buildfile() on them
  // in order.  any files in *already_seen will not be added, and each file
  // added will be inserted into *already_seen.  if include_source_code_info is
  // true then include the source code information in the filedescriptorprotos.
  static void gettransitivedependencies(
      const filedescriptor* file,
      bool include_source_code_info,
      set<const filedescriptor*>* already_seen,
      repeatedptrfield<filedescriptorproto>* output);

  // -----------------------------------------------------------------

  // the name of the executable as invoked (i.e. argv[0]).
  string executable_name_;

  // version info set with setversioninfo().
  string version_info_;

  // registered generators.
  struct generatorinfo {
    string flag_name;
    string option_flag_name;
    codegenerator* generator;
    string help_text;
  };
  typedef map<string, generatorinfo> generatormap;
  generatormap generators_by_flag_name_;
  generatormap generators_by_option_name_;
  // a map from generator names to the parameters specified using the option
  // flag. for example, if the user invokes the compiler with:
  //   protoc --foo_out=outputdir --foo_opt=enable_bar ...
  // then there will be an entry ("--foo_out", "enable_bar") in this map.
  map<string, string> generator_parameters_;

  // see allowplugins().  if this is empty, plugins aren't allowed.
  string plugin_prefix_;

  // maps specific plugin names to files.  when executing a plugin, this map
  // is searched first to find the plugin executable.  if not found here, the
  // path (or other os-specific search strategy) is searched.
  map<string, string> plugins_;

  // stuff parsed from command line.
  enum mode {
    mode_compile,  // normal mode:  parse .proto files and compile them.
    mode_encode,   // --encode:  read text from stdin, write binary to stdout.
    mode_decode    // --decode:  read binary from stdin, write text to stdout.
  };

  mode mode_;

  enum errorformat {
    error_format_gcc,   // gcc error output format (default).
    error_format_msvs   // visual studio output (--error_format=msvs).
  };

  errorformat error_format_;

  vector<pair<string, string> > proto_path_;  // search path for proto files.
  vector<string> input_files_;                // names of the input proto files.

  // output_directives_ lists all the files we are supposed to output and what
  // generator to use for each.
  struct outputdirective {
    string name;                // e.g. "--foo_out"
    codegenerator* generator;   // null for plugins
    string parameter;
    string output_location;
  };
  vector<outputdirective> output_directives_;

  // when using --encode or --decode, this names the type we are encoding or
  // decoding.  (empty string indicates --decode_raw.)
  string codec_type_;

  // if --descriptor_set_out was given, this is the filename to which the
  // filedescriptorset should be written.  otherwise, empty.
  string descriptor_set_name_;

  // true if --include_imports was given, meaning that we should
  // write all transitive dependencies to the descriptorset.  otherwise, only
  // the .proto files listed on the command-line are added.
  bool imports_in_descriptor_set_;

  // true if --include_source_info was given, meaning that we should not strip
  // sourcecodeinfo from the descriptorset.
  bool source_info_in_descriptor_set_;

  // was the --disallow_services flag used?
  bool disallow_services_;

  // see setinputsareprotopathrelative().
  bool inputs_are_proto_path_relative_;

  google_disallow_evil_constructors(commandlineinterface);
};

}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_command_line_interface_h__
