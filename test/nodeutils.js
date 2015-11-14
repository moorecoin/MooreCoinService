var async = require("async");
var fs	  = require("fs");
var path  = require("path");

var utils  = require("ripple-lib").utils;

// empty a directory.
// done(err) : err = true if an error occured.
var emptypath = function(dirpath, done) {
  fs.readdir(dirpath, function(err, files) {
    if (err) {
      done(err);
    }
    else {
      async.some(files.map(function(f) { return path.join(dirpath, f); }), rmpath, done);
    }
  });
};

// make a directory and sub-directories.
var mkpath = function(dirpath, mode, done) {
  fs.mkdir(dirpath, typeof mode === "string" ? parseint(mode, 8) : mode, function(e) {
      if (!e ||  e.code === "eexist") {
	// created or already exists, done.
	done();
      }
      else if (e.code === "enoent") {
	// missing sub dir.

	mkpath(path.dirname(dirpath), mode, function(e) {
	    if (e) {
	      throw e;
	    }
	    else {
	      mkpath(dirpath, mode, done);
	    }
	  });
      }
      else {
	throw e;
      }
  });
};

// create directory if needed and empty if needed.
var resetpath = function(dirpath, mode, done) {
  mkpath(dirpath, mode, function(e) {
      if (e) {
	done(e);
      }
      else {
	emptypath(dirpath, done);
      }
    });
};

// remove path recursively.
// done(err)
var rmpath = function(dirpath, done) {
//  console.log("rmpath: %s", dirpath);

  fs.lstat(dirpath, function(err, stats) {
      if (err && err.code == "enoent") {
	done();
      }
      if (err) {
	done(err);
      }
      else if (stats.isdirectory()) {
	emptypath(dirpath, function(e) {
	    if (e) {
	      done(e);
	    }
	    else {
//	      console.log("rmdir: %s", dirpath); done();
	      fs.rmdir(dirpath, done);
	    }
	  });
      }
      else {
//	console.log("unlink: %s", dirpath); done();
	fs.unlink(dirpath, done);
      }
    });
};

exports.mkpath	    = mkpath;
exports.resetpath   = resetpath;
exports.rmpath	    = rmpath;

// vim:sw=2:sts=2:ts=8:et
