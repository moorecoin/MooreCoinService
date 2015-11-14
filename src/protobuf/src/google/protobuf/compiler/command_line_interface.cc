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

#include <google/protobuf/compiler/command_line_interface.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _msc_ver
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <iostream>
#include <ctype.h>

#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.pb.h>
#include <google/protobuf/compiler/subprocess.h>
#include <google/protobuf/compiler/zip_writer.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/stubs/map-util.h>
#include <google/protobuf/stubs/stl_util.h>


namespace google {
namespace protobuf {
namespace compiler {

#if defined(_win32)
#define mkdir(name, mode) mkdir(name)
#ifndef w_ok
#define w_ok 02  // not defined by msvc for whatever reason
#endif
#ifndef f_ok
#define f_ok 00  // not defined by msvc for whatever reason
#endif
#ifndef stdin_fileno
#define stdin_fileno 0
#endif
#ifndef stdout_fileno
#define stdout_fileno 1
#endif
#endif

#ifndef o_binary
#ifdef _o_binary
#define o_binary _o_binary
#else
#define o_binary 0     // if this isn't defined, the platform doesn't need it.
#endif
#endif

namespace {
#if defined(_win32) && !defined(__cygwin__)
static const char* kpathseparator = ";";
#else
static const char* kpathseparator = ":";
#endif

// returns true if the text looks like a windows-style absolute path, starting
// with a drive letter.  example:  "c:\foo".  todo(kenton):  share this with
// copy in importer.cc?
static bool iswindowsabsolutepath(const string& text) {
#if defined(_win32) || defined(__cygwin__)
  return text.size() >= 3 && text[1] == ':' &&
         isalpha(text[0]) &&
         (text[2] == '/' || text[2] == '\\') &&
         text.find_last_of(':') == 1;
#else
  return false;
#endif
}

void setfdtotextmode(int fd) {
#ifdef _win32
  if (_setmode(fd, _o_text) == -1) {
    // this should never happen, i think.
    google_log(warning) << "_setmode(" << fd << ", _o_text): " << strerror(errno);
  }
#endif
  // (text and binary are the same on non-windows platforms.)
}

void setfdtobinarymode(int fd) {
#ifdef _win32
  if (_setmode(fd, _o_binary) == -1) {
    // this should never happen, i think.
    google_log(warning) << "_setmode(" << fd << ", _o_binary): " << strerror(errno);
  }
#endif
  // (text and binary are the same on non-windows platforms.)
}

void addtrailingslash(string* path) {
  if (!path->empty() && path->at(path->size() - 1) != '/') {
    path->push_back('/');
  }
}

bool verifydirectoryexists(const string& path) {
  if (path.empty()) return true;

  if (access(path.c_str(), f_ok) == -1) {
    cerr << path << ": " << strerror(errno) << endl;
    return false;
  } else {
    return true;
  }
}

// try to create the parent directory of the given file, creating the parent's
// parent if necessary, and so on.  the full file name is actually
// (prefix + filename), but we assume |prefix| already exists and only create
// directories listed in |filename|.
bool trycreateparentdirectory(const string& prefix, const string& filename) {
  // recursively create parent directories to the output file.
  vector<string> parts;
  splitstringusing(filename, "/", &parts);
  string path_so_far = prefix;
  for (int i = 0; i < parts.size() - 1; i++) {
    path_so_far += parts[i];
    if (mkdir(path_so_far.c_str(), 0777) != 0) {
      if (errno != eexist) {
        cerr << filename << ": while trying to create directory "
             << path_so_far << ": " << strerror(errno) << endl;
        return false;
      }
    }
    path_so_far += '/';
  }

  return true;
}

}  // namespace

// a multifileerrorcollector that prints errors to stderr.
class commandlineinterface::errorprinter : public multifileerrorcollector,
                                           public io::errorcollector {
 public:
  errorprinter(errorformat format, disksourcetree *tree = null)
    : format_(format), tree_(tree) {}
  ~errorprinter() {}

  // implements multifileerrorcollector ------------------------------
  void adderror(const string& filename, int line, int column,
                const string& message) {

    // print full path when running under msvs
    string dfile;
    if (format_ == commandlineinterface::error_format_msvs &&
        tree_ != null &&
        tree_->virtualfiletodiskfile(filename, &dfile)) {
      cerr << dfile;
    } else {
      cerr << filename;
    }

    // users typically expect 1-based line/column numbers, so we add 1
    // to each here.
    if (line != -1) {
      // allow for both gcc- and visual-studio-compatible output.
      switch (format_) {
        case commandlineinterface::error_format_gcc:
          cerr << ":" << (line + 1) << ":" << (column + 1);
          break;
        case commandlineinterface::error_format_msvs:
          cerr << "(" << (line + 1) << ") : error in column=" << (column + 1);
          break;
      }
    }

    cerr << ": " << message << endl;
  }

  // implements io::errorcollector -----------------------------------
  void adderror(int line, int column, const string& message) {
    adderror("input", line, column, message);
  }

 private:
  const errorformat format_;
  disksourcetree *tree_;
};

// -------------------------------------------------------------------

// a generatorcontext implementation that buffers files in memory, then dumps
// them all to disk on demand.
class commandlineinterface::generatorcontextimpl : public generatorcontext {
 public:
  generatorcontextimpl(const vector<const filedescriptor*>& parsed_files);
  ~generatorcontextimpl();

  // write all files in the directory to disk at the given output location,
  // which must end in a '/'.
  bool writealltodisk(const string& prefix);

  // write the contents of this directory to a zip-format archive with the
  // given name.
  bool writealltozip(const string& filename);

  // add a boilerplate meta-inf/manifest.mf file as required by the java jar
  // format, unless one has already been written.
  void addjarmanifest();

  // implements generatorcontext --------------------------------------
  io::zerocopyoutputstream* open(const string& filename);
  io::zerocopyoutputstream* openforinsert(
      const string& filename, const string& insertion_point);
  void listparsedfiles(vector<const filedescriptor*>* output) {
    *output = parsed_files_;
  }

 private:
  friend class memoryoutputstream;

  // map instead of hash_map so that files are written in order (good when
  // writing zips).
  map<string, string*> files_;
  const vector<const filedescriptor*>& parsed_files_;
  bool had_error_;
};

class commandlineinterface::memoryoutputstream
    : public io::zerocopyoutputstream {
 public:
  memoryoutputstream(generatorcontextimpl* directory, const string& filename);
  memoryoutputstream(generatorcontextimpl* directory, const string& filename,
                     const string& insertion_point);
  virtual ~memoryoutputstream();

  // implements zerocopyoutputstream ---------------------------------
  virtual bool next(void** data, int* size) { return inner_->next(data, size); }
  virtual void backup(int count)            {        inner_->backup(count);    }
  virtual int64 bytecount() const           { return inner_->bytecount();      }

 private:
  // where to insert the string when it's done.
  generatorcontextimpl* directory_;
  string filename_;
  string insertion_point_;

  // the string we're building.
  string data_;

  // stringoutputstream writing to data_.
  scoped_ptr<io::stringoutputstream> inner_;
};

// -------------------------------------------------------------------

commandlineinterface::generatorcontextimpl::generatorcontextimpl(
    const vector<const filedescriptor*>& parsed_files)
    : parsed_files_(parsed_files),
      had_error_(false) {
}

commandlineinterface::generatorcontextimpl::~generatorcontextimpl() {
  stldeletevalues(&files_);
}

bool commandlineinterface::generatorcontextimpl::writealltodisk(
    const string& prefix) {
  if (had_error_) {
    return false;
  }

  if (!verifydirectoryexists(prefix)) {
    return false;
  }

  for (map<string, string*>::const_iterator iter = files_.begin();
       iter != files_.end(); ++iter) {
    const string& relative_filename = iter->first;
    const char* data = iter->second->data();
    int size = iter->second->size();

    if (!trycreateparentdirectory(prefix, relative_filename)) {
      return false;
    }
    string filename = prefix + relative_filename;

    // create the output file.
    int file_descriptor;
    do {
      file_descriptor =
        open(filename.c_str(), o_wronly | o_creat | o_trunc | o_binary, 0666);
    } while (file_descriptor < 0 && errno == eintr);

    if (file_descriptor < 0) {
      int error = errno;
      cerr << filename << ": " << strerror(error);
      return false;
    }

    // write the file.
    while (size > 0) {
      int write_result;
      do {
        write_result = write(file_descriptor, data, size);
      } while (write_result < 0 && errno == eintr);

      if (write_result <= 0) {
        // write error.

        // fixme(kenton):  according to the man page, if write() returns zero,
        //   there was no error; write() simply did not write anything.  it's
        //   unclear under what circumstances this might happen, but presumably
        //   errno won't be set in this case.  i am confused as to how such an
        //   event should be handled.  for now i'm treating it as an error,
        //   since retrying seems like it could lead to an infinite loop.  i
        //   suspect this never actually happens anyway.

        if (write_result < 0) {
          int error = errno;
          cerr << filename << ": write: " << strerror(error);
        } else {
          cerr << filename << ": write() returned zero?" << endl;
        }
        return false;
      }

      data += write_result;
      size -= write_result;
    }

    if (close(file_descriptor) != 0) {
      int error = errno;
      cerr << filename << ": close: " << strerror(error);
      return false;
    }
  }

  return true;
}

bool commandlineinterface::generatorcontextimpl::writealltozip(
    const string& filename) {
  if (had_error_) {
    return false;
  }

  // create the output file.
  int file_descriptor;
  do {
    file_descriptor =
      open(filename.c_str(), o_wronly | o_creat | o_trunc | o_binary, 0666);
  } while (file_descriptor < 0 && errno == eintr);

  if (file_descriptor < 0) {
    int error = errno;
    cerr << filename << ": " << strerror(error);
    return false;
  }

  // create the zipwriter
  io::fileoutputstream stream(file_descriptor);
  zipwriter zip_writer(&stream);

  for (map<string, string*>::const_iterator iter = files_.begin();
       iter != files_.end(); ++iter) {
    zip_writer.write(iter->first, *iter->second);
  }

  zip_writer.writedirectory();

  if (stream.geterrno() != 0) {
    cerr << filename << ": " << strerror(stream.geterrno()) << endl;
  }

  if (!stream.close()) {
    cerr << filename << ": " << strerror(stream.geterrno()) << endl;
  }

  return true;
}

void commandlineinterface::generatorcontextimpl::addjarmanifest() {
  string** map_slot = &files_["meta-inf/manifest.mf"];
  if (*map_slot == null) {
    *map_slot = new string(
        "manifest-version: 1.0\n"
        "created-by: 1.6.0 (protoc)\n"
        "\n");
  }
}

io::zerocopyoutputstream* commandlineinterface::generatorcontextimpl::open(
    const string& filename) {
  return new memoryoutputstream(this, filename);
}

io::zerocopyoutputstream*
commandlineinterface::generatorcontextimpl::openforinsert(
    const string& filename, const string& insertion_point) {
  return new memoryoutputstream(this, filename, insertion_point);
}

// -------------------------------------------------------------------

commandlineinterface::memoryoutputstream::memoryoutputstream(
    generatorcontextimpl* directory, const string& filename)
    : directory_(directory),
      filename_(filename),
      inner_(new io::stringoutputstream(&data_)) {
}

commandlineinterface::memoryoutputstream::memoryoutputstream(
    generatorcontextimpl* directory, const string& filename,
    const string& insertion_point)
    : directory_(directory),
      filename_(filename),
      insertion_point_(insertion_point),
      inner_(new io::stringoutputstream(&data_)) {
}

commandlineinterface::memoryoutputstream::~memoryoutputstream() {
  // make sure all data has been written.
  inner_.reset();

  // insert into the directory.
  string** map_slot = &directory_->files_[filename_];

  if (insertion_point_.empty()) {
    // this was just a regular open().
    if (*map_slot != null) {
      cerr << filename_ << ": tried to write the same file twice." << endl;
      directory_->had_error_ = true;
      return;
    }

    *map_slot = new string;
    (*map_slot)->swap(data_);
  } else {
    // this was an openforinsert().

    // if the data doens't end with a clean line break, add one.
    if (!data_.empty() && data_[data_.size() - 1] != '\n') {
      data_.push_back('\n');
    }

    // find the file we are going to insert into.
    if (*map_slot == null) {
      cerr << filename_ << ": tried to insert into file that doesn't exist."
           << endl;
      directory_->had_error_ = true;
      return;
    }
    string* target = *map_slot;

    // find the insertion point.
    string magic_string = strings::substitute(
        "@@protoc_insertion_point($0)", insertion_point_);
    string::size_type pos = target->find(magic_string);

    if (pos == string::npos) {
      cerr << filename_ << ": insertion point \"" << insertion_point_
           << "\" not found." << endl;
      directory_->had_error_ = true;
      return;
    }

    // seek backwards to the beginning of the line, which is where we will
    // insert the data.  note that this has the effect of pushing the insertion
    // point down, so the data is inserted before it.  this is intentional
    // because it means that multiple insertions at the same point will end
    // up in the expected order in the final output.
    pos = target->find_last_of('\n', pos);
    if (pos == string::npos) {
      // insertion point is on the first line.
      pos = 0;
    } else {
      // advance to character after '\n'.
      ++pos;
    }

    // extract indent.
    string indent_(*target, pos, target->find_first_not_of(" \t", pos) - pos);

    if (indent_.empty()) {
      // no indent.  this makes things easier.
      target->insert(pos, data_);
    } else {
      // calculate how much space we need.
      int indent_size = 0;
      for (int i = 0; i < data_.size(); i++) {
        if (data_[i] == '\n') indent_size += indent_.size();
      }

      // make a hole for it.
      target->insert(pos, data_.size() + indent_size, '\0');

      // now copy in the data.
      string::size_type data_pos = 0;
      char* target_ptr = string_as_array(target) + pos;
      while (data_pos < data_.size()) {
        // copy indent.
        memcpy(target_ptr, indent_.data(), indent_.size());
        target_ptr += indent_.size();

        // copy line from data_.
        // we already guaranteed that data_ ends with a newline (above), so this
        // search can't fail.
        string::size_type line_length =
            data_.find_first_of('\n', data_pos) + 1 - data_pos;
        memcpy(target_ptr, data_.data() + data_pos, line_length);
        target_ptr += line_length;
        data_pos += line_length;
      }

      google_check_eq(target_ptr,
          string_as_array(target) + pos + data_.size() + indent_size);
    }
  }
}

// ===================================================================

commandlineinterface::commandlineinterface()
  : mode_(mode_compile),
    error_format_(error_format_gcc),
    imports_in_descriptor_set_(false),
    source_info_in_descriptor_set_(false),
    disallow_services_(false),
    inputs_are_proto_path_relative_(false) {}
commandlineinterface::~commandlineinterface() {}

void commandlineinterface::registergenerator(const string& flag_name,
                                             codegenerator* generator,
                                             const string& help_text) {
  generatorinfo info;
  info.flag_name = flag_name;
  info.generator = generator;
  info.help_text = help_text;
  generators_by_flag_name_[flag_name] = info;
}

void commandlineinterface::registergenerator(const string& flag_name,
                                             const string& option_flag_name,
                                             codegenerator* generator,
                                             const string& help_text) {
  generatorinfo info;
  info.flag_name = flag_name;
  info.option_flag_name = option_flag_name;
  info.generator = generator;
  info.help_text = help_text;
  generators_by_flag_name_[flag_name] = info;
  generators_by_option_name_[option_flag_name] = info;
}

void commandlineinterface::allowplugins(const string& exe_name_prefix) {
  plugin_prefix_ = exe_name_prefix;
}

int commandlineinterface::run(int argc, const char* const argv[]) {
  clear();
  switch (parsearguments(argc, argv)) {
    case parse_argument_done_and_exit:
      return 0;
    case parse_argument_fail:
      return 1;
    case parse_argument_done_and_continue:
      break;
  }

  // set up the source tree.
  disksourcetree source_tree;
  for (int i = 0; i < proto_path_.size(); i++) {
    source_tree.mappath(proto_path_[i].first, proto_path_[i].second);
  }

  // map input files to virtual paths if necessary.
  if (!inputs_are_proto_path_relative_) {
    if (!makeinputsbeprotopathrelative(&source_tree)) {
      return 1;
    }
  }

  // allocate the importer.
  errorprinter error_collector(error_format_, &source_tree);
  importer importer(&source_tree, &error_collector);

  vector<const filedescriptor*> parsed_files;

  // parse each file.
  for (int i = 0; i < input_files_.size(); i++) {
    // import the file.
    const filedescriptor* parsed_file = importer.import(input_files_[i]);
    if (parsed_file == null) return 1;
    parsed_files.push_back(parsed_file);

    // enforce --disallow_services.
    if (disallow_services_ && parsed_file->service_count() > 0) {
      cerr << parsed_file->name() << ": this file contains services, but "
              "--disallow_services was used." << endl;
      return 1;
    }
  }

  // we construct a separate generatorcontext for each output location.  note
  // that two code generators may output to the same location, in which case
  // they should share a single generatorcontext so that openforinsert() works.
  typedef hash_map<string, generatorcontextimpl*> generatorcontextmap;
  generatorcontextmap output_directories;

  // generate output.
  if (mode_ == mode_compile) {
    for (int i = 0; i < output_directives_.size(); i++) {
      string output_location = output_directives_[i].output_location;
      if (!hassuffixstring(output_location, ".zip") &&
          !hassuffixstring(output_location, ".jar")) {
        addtrailingslash(&output_location);
      }
      generatorcontextimpl** map_slot = &output_directories[output_location];

      if (*map_slot == null) {
        // first time we've seen this output location.
        *map_slot = new generatorcontextimpl(parsed_files);
      }

      if (!generateoutput(parsed_files, output_directives_[i], *map_slot)) {
        stldeletevalues(&output_directories);
        return 1;
      }
    }
  }

  // write all output to disk.
  for (generatorcontextmap::iterator iter = output_directories.begin();
       iter != output_directories.end(); ++iter) {
    const string& location = iter->first;
    generatorcontextimpl* directory = iter->second;
    if (hassuffixstring(location, "/")) {
      if (!directory->writealltodisk(location)) {
        stldeletevalues(&output_directories);
        return 1;
      }
    } else {
      if (hassuffixstring(location, ".jar")) {
        directory->addjarmanifest();
      }

      if (!directory->writealltozip(location)) {
        stldeletevalues(&output_directories);
        return 1;
      }
    }
  }

  stldeletevalues(&output_directories);

  if (!descriptor_set_name_.empty()) {
    if (!writedescriptorset(parsed_files)) {
      return 1;
    }
  }

  if (mode_ == mode_encode || mode_ == mode_decode) {
    if (codec_type_.empty()) {
      // hack:  define an emptymessage type to use for decoding.
      descriptorpool pool;
      filedescriptorproto file;
      file.set_name("empty_message.proto");
      file.add_message_type()->set_name("emptymessage");
      google_check(pool.buildfile(file) != null);
      codec_type_ = "emptymessage";
      if (!encodeordecode(&pool)) {
        return 1;
      }
    } else {
      if (!encodeordecode(importer.pool())) {
        return 1;
      }
    }
  }

  return 0;
}

void commandlineinterface::clear() {
  // clear all members that are set by run().  note that we must not clear
  // members which are set by other methods before run() is called.
  executable_name_.clear();
  proto_path_.clear();
  input_files_.clear();
  output_directives_.clear();
  codec_type_.clear();
  descriptor_set_name_.clear();

  mode_ = mode_compile;
  imports_in_descriptor_set_ = false;
  source_info_in_descriptor_set_ = false;
  disallow_services_ = false;
}

bool commandlineinterface::makeinputsbeprotopathrelative(
    disksourcetree* source_tree) {
  for (int i = 0; i < input_files_.size(); i++) {
    string virtual_file, shadowing_disk_file;
    switch (source_tree->diskfiletovirtualfile(
        input_files_[i], &virtual_file, &shadowing_disk_file)) {
      case disksourcetree::success:
        input_files_[i] = virtual_file;
        break;
      case disksourcetree::shadowed:
        cerr << input_files_[i] << ": input is shadowed in the --proto_path "
                "by \"" << shadowing_disk_file << "\".  either use the latter "
                "file as your input or reorder the --proto_path so that the "
                "former file's location comes first." << endl;
        return false;
      case disksourcetree::cannot_open:
        cerr << input_files_[i] << ": " << strerror(errno) << endl;
        return false;
      case disksourcetree::no_mapping:
        // first check if the file exists at all.
        if (access(input_files_[i].c_str(), f_ok) < 0) {
          // file does not even exist.
          cerr << input_files_[i] << ": " << strerror(enoent) << endl;
        } else {
          cerr << input_files_[i] << ": file does not reside within any path "
                  "specified using --proto_path (or -i).  you must specify a "
                  "--proto_path which encompasses this file.  note that the "
                  "proto_path must be an exact prefix of the .proto file "
                  "names -- protoc is too dumb to figure out when two paths "
                  "(e.g. absolute and relative) are equivalent (it's harder "
                  "than you think)." << endl;
        }
        return false;
    }
  }

  return true;
}

commandlineinterface::parseargumentstatus
commandlineinterface::parsearguments(int argc, const char* const argv[]) {
  executable_name_ = argv[0];

  // iterate through all arguments and parse them.
  for (int i = 1; i < argc; i++) {
    string name, value;

    if (parseargument(argv[i], &name, &value)) {
      // returned true => use the next argument as the flag value.
      if (i + 1 == argc || argv[i+1][0] == '-') {
        cerr << "missing value for flag: " << name << endl;
        if (name == "--decode") {
          cerr << "to decode an unknown message, use --decode_raw." << endl;
        }
        return parse_argument_fail;
      } else {
        ++i;
        value = argv[i];
      }
    }

    parseargumentstatus status = interpretargument(name, value);
    if (status != parse_argument_done_and_continue)
      return status;
  }

  // if no --proto_path was given, use the current working directory.
  if (proto_path_.empty()) {
    // don't use make_pair as the old/default standard library on solaris
    // doesn't support it without explicit template parameters, which are
    // incompatible with c++0x's make_pair.
    proto_path_.push_back(pair<string, string>("", "."));
  }

  // check some errror cases.
  bool decoding_raw = (mode_ == mode_decode) && codec_type_.empty();
  if (decoding_raw && !input_files_.empty()) {
    cerr << "when using --decode_raw, no input files should be given." << endl;
    return parse_argument_fail;
  } else if (!decoding_raw && input_files_.empty()) {
    cerr << "missing input file." << endl;
    return parse_argument_fail;
  }
  if (mode_ == mode_compile && output_directives_.empty() &&
      descriptor_set_name_.empty()) {
    cerr << "missing output directives." << endl;
    return parse_argument_fail;
  }
  if (imports_in_descriptor_set_ && descriptor_set_name_.empty()) {
    cerr << "--include_imports only makes sense when combined with "
            "--descriptor_set_out." << endl;
  }
  if (source_info_in_descriptor_set_ && descriptor_set_name_.empty()) {
    cerr << "--include_source_info only makes sense when combined with "
            "--descriptor_set_out." << endl;
  }

  return parse_argument_done_and_continue;
}

bool commandlineinterface::parseargument(const char* arg,
                                         string* name, string* value) {
  bool parsed_value = false;

  if (arg[0] != '-') {
    // not a flag.
    name->clear();
    parsed_value = true;
    *value = arg;
  } else if (arg[1] == '-') {
    // two dashes:  multi-character name, with '=' separating name and
    //   value.
    const char* equals_pos = strchr(arg, '=');
    if (equals_pos != null) {
      *name = string(arg, equals_pos - arg);
      *value = equals_pos + 1;
      parsed_value = true;
    } else {
      *name = arg;
    }
  } else {
    // one dash:  one-character name, all subsequent characters are the
    //   value.
    if (arg[1] == '\0') {
      // arg is just "-".  we treat this as an input file, except that at
      // present this will just lead to a "file not found" error.
      name->clear();
      *value = arg;
      parsed_value = true;
    } else {
      *name = string(arg, 2);
      *value = arg + 2;
      parsed_value = !value->empty();
    }
  }

  // need to return true iff the next arg should be used as the value for this
  // one, false otherwise.

  if (parsed_value) {
    // we already parsed a value for this flag.
    return false;
  }

  if (*name == "-h" || *name == "--help" ||
      *name == "--disallow_services" ||
      *name == "--include_imports" ||
      *name == "--include_source_info" ||
      *name == "--version" ||
      *name == "--decode_raw") {
    // hack:  these are the only flags that don't take a value.
    //   they probably should not be hard-coded like this but for now it's
    //   not worth doing better.
    return false;
  }

  // next argument is the flag value.
  return true;
}

commandlineinterface::parseargumentstatus
commandlineinterface::interpretargument(const string& name,
                                        const string& value) {
  if (name.empty()) {
    // not a flag.  just a filename.
    if (value.empty()) {
      cerr << "you seem to have passed an empty string as one of the "
              "arguments to " << executable_name_ << ".  this is actually "
              "sort of hard to do.  congrats.  unfortunately it is not valid "
              "input so the program is going to die now." << endl;
      return parse_argument_fail;
    }

    input_files_.push_back(value);

  } else if (name == "-i" || name == "--proto_path") {
    // java's -classpath (and some other languages) delimits path components
    // with colons.  let's accept that syntax too just to make things more
    // intuitive.
    vector<string> parts;
    splitstringusing(value, kpathseparator, &parts);

    for (int i = 0; i < parts.size(); i++) {
      string virtual_path;
      string disk_path;

      string::size_type equals_pos = parts[i].find_first_of('=');
      if (equals_pos == string::npos) {
        virtual_path = "";
        disk_path = parts[i];
      } else {
        virtual_path = parts[i].substr(0, equals_pos);
        disk_path = parts[i].substr(equals_pos + 1);
      }

      if (disk_path.empty()) {
        cerr << "--proto_path passed empty directory name.  (use \".\" for "
                "current directory.)" << endl;
        return parse_argument_fail;
      }

      // make sure disk path exists, warn otherwise.
      if (access(disk_path.c_str(), f_ok) < 0) {
        cerr << disk_path << ": warning: directory does not exist." << endl;
      }

      // don't use make_pair as the old/default standard library on solaris
      // doesn't support it without explicit template parameters, which are
      // incompatible with c++0x's make_pair.
      proto_path_.push_back(pair<string, string>(virtual_path, disk_path));
    }

  } else if (name == "-o" || name == "--descriptor_set_out") {
    if (!descriptor_set_name_.empty()) {
      cerr << name << " may only be passed once." << endl;
      return parse_argument_fail;
    }
    if (value.empty()) {
      cerr << name << " requires a non-empty value." << endl;
      return parse_argument_fail;
    }
    if (mode_ != mode_compile) {
      cerr << "cannot use --encode or --decode and generate descriptors at the "
              "same time." << endl;
      return parse_argument_fail;
    }
    descriptor_set_name_ = value;

  } else if (name == "--include_imports") {
    if (imports_in_descriptor_set_) {
      cerr << name << " may only be passed once." << endl;
      return parse_argument_fail;
    }
    imports_in_descriptor_set_ = true;

  } else if (name == "--include_source_info") {
    if (source_info_in_descriptor_set_) {
      cerr << name << " may only be passed once." << endl;
      return parse_argument_fail;
    }
    source_info_in_descriptor_set_ = true;

  } else if (name == "-h" || name == "--help") {
    printhelptext();
    return parse_argument_done_and_exit;  // exit without running compiler.

  } else if (name == "--version") {
    if (!version_info_.empty()) {
      cout << version_info_ << endl;
    }
    cout << "libprotoc "
         << protobuf::internal::versionstring(google_protobuf_version)
         << endl;
    return parse_argument_done_and_exit;  // exit without running compiler.

  } else if (name == "--disallow_services") {
    disallow_services_ = true;

  } else if (name == "--encode" || name == "--decode" ||
             name == "--decode_raw") {
    if (mode_ != mode_compile) {
      cerr << "only one of --encode and --decode can be specified." << endl;
      return parse_argument_fail;
    }
    if (!output_directives_.empty() || !descriptor_set_name_.empty()) {
      cerr << "cannot use " << name
           << " and generate code or descriptors at the same time." << endl;
      return parse_argument_fail;
    }

    mode_ = (name == "--encode") ? mode_encode : mode_decode;

    if (value.empty() && name != "--decode_raw") {
      cerr << "type name for " << name << " cannot be blank." << endl;
      if (name == "--decode") {
        cerr << "to decode an unknown message, use --decode_raw." << endl;
      }
      return parse_argument_fail;
    } else if (!value.empty() && name == "--decode_raw") {
      cerr << "--decode_raw does not take a parameter." << endl;
      return parse_argument_fail;
    }

    codec_type_ = value;

  } else if (name == "--error_format") {
    if (value == "gcc") {
      error_format_ = error_format_gcc;
    } else if (value == "msvs") {
      error_format_ = error_format_msvs;
    } else {
      cerr << "unknown error format: " << value << endl;
      return parse_argument_fail;
    }

  } else if (name == "--plugin") {
    if (plugin_prefix_.empty()) {
      cerr << "this compiler does not support plugins." << endl;
      return parse_argument_fail;
    }

    string plugin_name;
    string path;

    string::size_type equals_pos = value.find_first_of('=');
    if (equals_pos == string::npos) {
      // use the basename of the file.
      string::size_type slash_pos = value.find_last_of('/');
      if (slash_pos == string::npos) {
        plugin_name = value;
      } else {
        plugin_name = value.substr(slash_pos + 1);
      }
      path = value;
    } else {
      plugin_name = value.substr(0, equals_pos);
      path = value.substr(equals_pos + 1);
    }

    plugins_[plugin_name] = path;

  } else {
    // some other flag.  look it up in the generators list.
    const generatorinfo* generator_info =
        findornull(generators_by_flag_name_, name);
    if (generator_info == null &&
        (plugin_prefix_.empty() || !hassuffixstring(name, "_out"))) {
      // check if it's a generator option flag.
      generator_info = findornull(generators_by_option_name_, name);
      if (generator_info == null) {
        cerr << "unknown flag: " << name << endl;
        return parse_argument_fail;
      } else {
        string* parameters = &generator_parameters_[generator_info->flag_name];
        if (!parameters->empty()) {
          parameters->append(",");
        }
        parameters->append(value);
      }
    } else {
      // it's an output flag.  add it to the output directives.
      if (mode_ != mode_compile) {
        cerr << "cannot use --encode or --decode and generate code at the "
                "same time." << endl;
        return parse_argument_fail;
      }

      outputdirective directive;
      directive.name = name;
      if (generator_info == null) {
        directive.generator = null;
      } else {
        directive.generator = generator_info->generator;
      }

      // split value at ':' to separate the generator parameter from the
      // filename.  however, avoid doing this if the colon is part of a valid
      // windows-style absolute path.
      string::size_type colon_pos = value.find_first_of(':');
      if (colon_pos == string::npos || iswindowsabsolutepath(value)) {
        directive.output_location = value;
      } else {
        directive.parameter = value.substr(0, colon_pos);
        directive.output_location = value.substr(colon_pos + 1);
      }

      output_directives_.push_back(directive);
    }
  }

  return parse_argument_done_and_continue;
}

void commandlineinterface::printhelptext() {
  // sorry for indentation here; line wrapping would be uglier.
  cerr <<
"usage: " << executable_name_ << " [option] proto_files\n"
"parse proto_files and generate output based on the options given:\n"
"  -ipath, --proto_path=path   specify the directory in which to search for\n"
"                              imports.  may be specified multiple times;\n"
"                              directories will be searched in order.  if not\n"
"                              given, the current working directory is used.\n"
"  --version                   show version info and exit.\n"
"  -h, --help                  show this text and exit.\n"
"  --encode=message_type       read a text-format message of the given type\n"
"                              from standard input and write it in binary\n"
"                              to standard output.  the message type must\n"
"                              be defined in proto_files or their imports.\n"
"  --decode=message_type       read a binary message of the given type from\n"
"                              standard input and write it in text format\n"
"                              to standard output.  the message type must\n"
"                              be defined in proto_files or their imports.\n"
"  --decode_raw                read an arbitrary protocol message from\n"
"                              standard input and write the raw tag/value\n"
"                              pairs in text format to standard output.  no\n"
"                              proto_files should be given when using this\n"
"                              flag.\n"
"  -ofile,                     writes a filedescriptorset (a protocol buffer,\n"
"    --descriptor_set_out=file defined in descriptor.proto) containing all of\n"
"                              the input files to file.\n"
"  --include_imports           when using --descriptor_set_out, also include\n"
"                              all dependencies of the input files in the\n"
"                              set, so that the set is self-contained.\n"
"  --include_source_info       when using --descriptor_set_out, do not strip\n"
"                              sourcecodeinfo from the filedescriptorproto.\n"
"                              this results in vastly larger descriptors that\n"
"                              include information about the original\n"
"                              location of each decl in the source file as\n"
"                              well as surrounding comments.\n"
"  --error_format=format       set the format in which to print errors.\n"
"                              format may be 'gcc' (the default) or 'msvs'\n"
"                              (microsoft visual studio format)." << endl;
  if (!plugin_prefix_.empty()) {
    cerr <<
"  --plugin=executable         specifies a plugin executable to use.\n"
"                              normally, protoc searches the path for\n"
"                              plugins, but you may specify additional\n"
"                              executables not in the path using this flag.\n"
"                              additionally, executable may be of the form\n"
"                              name=path, in which case the given plugin name\n"
"                              is mapped to the given executable even if\n"
"                              the executable's own name differs." << endl;
  }

  for (generatormap::iterator iter = generators_by_flag_name_.begin();
       iter != generators_by_flag_name_.end(); ++iter) {
    // fixme(kenton):  if the text is long enough it will wrap, which is ugly,
    //   but fixing this nicely (e.g. splitting on spaces) is probably more
    //   trouble than it's worth.
    cerr << "  " << iter->first << "=out_dir "
         << string(19 - iter->first.size(), ' ')  // spaces for alignment.
         << iter->second.help_text << endl;
  }
}

bool commandlineinterface::generateoutput(
    const vector<const filedescriptor*>& parsed_files,
    const outputdirective& output_directive,
    generatorcontext* generator_context) {
  // call the generator.
  string error;
  if (output_directive.generator == null) {
    // this is a plugin.
    google_check(hasprefixstring(output_directive.name, "--") &&
          hassuffixstring(output_directive.name, "_out"))
        << "bad name for plugin generator: " << output_directive.name;

    // strip the "--" and "_out" and add the plugin prefix.
    string plugin_name = plugin_prefix_ + "gen-" +
        output_directive.name.substr(2, output_directive.name.size() - 6);

    if (!generatepluginoutput(parsed_files, plugin_name,
                              output_directive.parameter,
                              generator_context, &error)) {
      cerr << output_directive.name << ": " << error << endl;
      return false;
    }
  } else {
    // regular generator.
    string parameters = output_directive.parameter;
    if (!generator_parameters_[output_directive.name].empty()) {
      if (!parameters.empty()) {
        parameters.append(",");
      }
      parameters.append(generator_parameters_[output_directive.name]);
    }
    for (int i = 0; i < parsed_files.size(); i++) {
      if (!output_directive.generator->generate(parsed_files[i], parameters,
                                                generator_context, &error)) {
        // generator returned an error.
        cerr << output_directive.name << ": " << parsed_files[i]->name() << ": "
             << error << endl;
        return false;
      }
    }
  }

  return true;
}

bool commandlineinterface::generatepluginoutput(
    const vector<const filedescriptor*>& parsed_files,
    const string& plugin_name,
    const string& parameter,
    generatorcontext* generator_context,
    string* error) {
  codegeneratorrequest request;
  codegeneratorresponse response;

  // build the request.
  if (!parameter.empty()) {
    request.set_parameter(parameter);
  }

  set<const filedescriptor*> already_seen;
  for (int i = 0; i < parsed_files.size(); i++) {
    request.add_file_to_generate(parsed_files[i]->name());
    gettransitivedependencies(parsed_files[i],
                              true,  // include source code info.
                              &already_seen, request.mutable_proto_file());
  }

  // invoke the plugin.
  subprocess subprocess;

  if (plugins_.count(plugin_name) > 0) {
    subprocess.start(plugins_[plugin_name], subprocess::exact_name);
  } else {
    subprocess.start(plugin_name, subprocess::search_path);
  }

  string communicate_error;
  if (!subprocess.communicate(request, &response, &communicate_error)) {
    *error = strings::substitute("$0: $1", plugin_name, communicate_error);
    return false;
  }

  // write the files.  we do this even if there was a generator error in order
  // to match the behavior of a compiled-in generator.
  scoped_ptr<io::zerocopyoutputstream> current_output;
  for (int i = 0; i < response.file_size(); i++) {
    const codegeneratorresponse::file& output_file = response.file(i);

    if (!output_file.insertion_point().empty()) {
      // open a file for insert.
      // we reset current_output to null first so that the old file is closed
      // before the new one is opened.
      current_output.reset();
      current_output.reset(generator_context->openforinsert(
          output_file.name(), output_file.insertion_point()));
    } else if (!output_file.name().empty()) {
      // starting a new file.  open it.
      // we reset current_output to null first so that the old file is closed
      // before the new one is opened.
      current_output.reset();
      current_output.reset(generator_context->open(output_file.name()));
    } else if (current_output == null) {
      *error = strings::substitute(
        "$0: first file chunk returned by plugin did not specify a file name.",
        plugin_name);
      return false;
    }

    // use codedoutputstream for convenience; otherwise we'd need to provide
    // our own buffer-copying loop.
    io::codedoutputstream writer(current_output.get());
    writer.writestring(output_file.content());
  }

  // check for errors.
  if (!response.error().empty()) {
    // generator returned an error.
    *error = response.error();
    return false;
  }

  return true;
}

bool commandlineinterface::encodeordecode(const descriptorpool* pool) {
  // look up the type.
  const descriptor* type = pool->findmessagetypebyname(codec_type_);
  if (type == null) {
    cerr << "type not defined: " << codec_type_ << endl;
    return false;
  }

  dynamicmessagefactory dynamic_factory(pool);
  scoped_ptr<message> message(dynamic_factory.getprototype(type)->new());

  if (mode_ == mode_encode) {
    setfdtotextmode(stdin_fileno);
    setfdtobinarymode(stdout_fileno);
  } else {
    setfdtobinarymode(stdin_fileno);
    setfdtotextmode(stdout_fileno);
  }

  io::fileinputstream in(stdin_fileno);
  io::fileoutputstream out(stdout_fileno);

  if (mode_ == mode_encode) {
    // input is text.
    errorprinter error_collector(error_format_);
    textformat::parser parser;
    parser.recorderrorsto(&error_collector);
    parser.allowpartialmessage(true);

    if (!parser.parse(&in, message.get())) {
      cerr << "failed to parse input." << endl;
      return false;
    }
  } else {
    // input is binary.
    if (!message->parsepartialfromzerocopystream(&in)) {
      cerr << "failed to parse input." << endl;
      return false;
    }
  }

  if (!message->isinitialized()) {
    cerr << "warning:  input message is missing required fields:  "
         << message->initializationerrorstring() << endl;
  }

  if (mode_ == mode_encode) {
    // output is binary.
    if (!message->serializepartialtozerocopystream(&out)) {
      cerr << "output: i/o error." << endl;
      return false;
    }
  } else {
    // output is text.
    if (!textformat::print(*message, &out)) {
      cerr << "output: i/o error." << endl;
      return false;
    }
  }

  return true;
}

bool commandlineinterface::writedescriptorset(
    const vector<const filedescriptor*> parsed_files) {
  filedescriptorset file_set;

  if (imports_in_descriptor_set_) {
    set<const filedescriptor*> already_seen;
    for (int i = 0; i < parsed_files.size(); i++) {
      gettransitivedependencies(parsed_files[i],
                                source_info_in_descriptor_set_,
                                &already_seen, file_set.mutable_file());
    }
  } else {
    for (int i = 0; i < parsed_files.size(); i++) {
      filedescriptorproto* file_proto = file_set.add_file();
      parsed_files[i]->copyto(file_proto);
      if (source_info_in_descriptor_set_) {
        parsed_files[i]->copysourcecodeinfoto(file_proto);
      }
    }
  }

  int fd;
  do {
    fd = open(descriptor_set_name_.c_str(),
              o_wronly | o_creat | o_trunc | o_binary, 0666);
  } while (fd < 0 && errno == eintr);

  if (fd < 0) {
    perror(descriptor_set_name_.c_str());
    return false;
  }

  io::fileoutputstream out(fd);
  if (!file_set.serializetozerocopystream(&out)) {
    cerr << descriptor_set_name_ << ": " << strerror(out.geterrno()) << endl;
    out.close();
    return false;
  }
  if (!out.close()) {
    cerr << descriptor_set_name_ << ": " << strerror(out.geterrno()) << endl;
    return false;
  }

  return true;
}

void commandlineinterface::gettransitivedependencies(
    const filedescriptor* file, bool include_source_code_info,
    set<const filedescriptor*>* already_seen,
    repeatedptrfield<filedescriptorproto>* output) {
  if (!already_seen->insert(file).second) {
    // already saw this file.  skip.
    return;
  }

  // add all dependencies.
  for (int i = 0; i < file->dependency_count(); i++) {
    gettransitivedependencies(file->dependency(i), include_source_code_info,
                              already_seen, output);
  }

  // add this file.
  filedescriptorproto* new_descriptor = output->add();
  file->copyto(new_descriptor);
  if (include_source_code_info) {
    file->copysourcecodeinfoto(new_descriptor);
  }
}


}  // namespace compiler
}  // namespace protobuf
}  // namespace google
