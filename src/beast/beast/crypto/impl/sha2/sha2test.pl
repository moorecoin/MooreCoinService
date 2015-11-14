#!/usr/bin/perl
#
# file:		sha2test.pl
# author:	aaron d. gifford - http://www.aarongifford.com/
# 
# copyright (c) 2001, aaron d. gifford
# all rights reserved.
#
# redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. neither the name of the copyright holder nor the names of contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
# 
# this software is provided by the author and contributor(s) ``as is'' and
# any express or implied warranties, including, but not limited to, the
# implied warranties of merchantability and fitness for a particular purpose
# are disclaimed.  in no event shall the author or contributor(s) be liable
# for any direct, indirect, incidental, special, exemplary, or consequential
# damages (including, but not limited to, procurement of substitute goods
# or services; loss of use, data, or profits; or business interruption)
# however caused and on any theory of liability, whether in contract, strict
# liability, or tort (including negligence or otherwise) arising in any way
# out of the use of this software, even if advised of the possibility of
# such damage.
#
# $id: sha2test.pl,v 1.1 2001/11/08 00:02:37 adg exp adg $
#

sub usage {
	my ($err) = shift(@_);

	print <<eom;
error:
	$err
usage:
	$0 [<options>] [<test-vector-info-file> [<test-vector-info-file> ...]]

options:
	-256	use sha-256 hashes during testing
	-384	use sha-384 hashes during testing
	-512	use sha-512 hashes during testing
	-all	use all three hashes during testing
	-c256 <command-spec>	specify a command to execute to generate a
				sha-256 hash.  be sure to include a '%'
				character which will be replaced by the
				test vector data filename containing the
				data to be hashed.  this command implies
				the -256 option.
	-c384 <command-spec>	specify a command to execute to generate a
				sha-384 hash.  see above.  implies -384.
	-c512 <command-spec>	specify a command to execute to generate a
				sha-512 hash.  see above.  implies -512.
	-call <command-spec>	specify a command to execute that will
				generate all three hashes at once and output
				the data in hexadecimal.  see above for
				information about the <command-spec>.
				this option implies the -all option, and
				also overrides any other command options if
				present.

by default, this program expects to execute the command ./sha2 within the
current working directory to generate all hashes.  if no test vector
information files are specified, this program expects to read a series of
files ending in ".info" within a subdirectory of the current working
directory called "testvectors".

eom
	exit(-1);
}

$c256 = $c384 = $c512 = $call = "";
$hashes = 0;
@files = ();

# read all command-line options and files:
while ($opt = shift(@argv)) {
	if ($opt =~ s/^\-//) {
		if ($opt eq "256") {
			$hashes |= 1;
		} elsif ($opt eq "384") {
			$hashes |= 2;
		} elsif ($opt eq "512") {
			$hashes |= 4;
		} elsif ($opt =~ /^all$/i) {
			$hashes = 7;
		} elsif ($opt =~ /^c256$/i) {
			$hashes |= 1;
			$opt = $c256 = shift(@argv);
			$opt =~ s/\s+.*$//;
			if (!$c256 || $c256 !~ /\%/ || !-x $opt) {
				usage("missing or invalid command specification for option -c256: $opt\n");
			}
		} elsif ($opt =~ /^c384$/i) {
			$hashes |= 2;
			$opt = $c384 = shift(@argv);
			$opt =~ s/\s+.*$//;
			if (!$c384 || $c384 !~ /\%/ || !-x $opt) {
				usage("missing or invalid command specification for option -c384: $opt\n");
			}
		} elsif ($opt =~ /^c512$/i) {
			$hashes |= 4;
			$opt = $c512 = shift(@argv);
			$opt =~ s/\s+.*$//;
			if (!$c512 || $c512 !~ /\%/ || !-x $opt) {
				usage("missing or invalid command specification for option -c512: $opt\n");
			}
		} elsif ($opt =~ /^call$/i) {
			$hashes = 7;
			$opt = $call = shift(@argv);
			$opt =~ s/\s+.*$//;
			if (!$call || $call !~ /\%/ || !-x $opt) {
				usage("missing or invalid command specification for option -call: $opt\n");
			}
		} else {
			usage("unknown/invalid option '$opt'\n");
		}
	} else {
		usage("invalid, nonexistent, or unreadable file '$opt': $!\n") if (!-f $opt);
		push(@files, $opt);
	}
}

# set up defaults:
if (!$call && !$c256 && !$c384 && !$c512) {
	$call = "./sha2 -all %";
	usage("required ./sha2 binary executable not found.\n") if (!-x "./sha2");
}
$hashes = 7 if (!$hashes);

# do some sanity checks:
usage("no command was supplied to generate sha-256 hashes.\n") if ($hashes & 1 == 1 && !$call && !$c256);
usage("no command was supplied to generate sha-384 hashes.\n") if ($hashes & 2 == 2 && !$call && !$c384);
usage("no command was supplied to generate sha-512 hashes.\n") if ($hashes & 4 == 4 && !$call && !$c512);

# default .info files:
if (scalar(@files) < 1) {
	opendir(dir, "testvectors") || usage("unable to scan directory 'testvectors' for vector information files: $!\n");
	@files = grep(/\.info$/, readdir(dir));
	closedir(dir);
	@files = map { s/^/testvectors\//; $_; } @files;
	@files = sort(@files);
}

# now read in each test vector information file:
foreach $file (@files) {
	$dir = $file;
	if ($file !~ /\//) {
		$dir = "./";
	} else {
		$dir =~ s/\/[^\/]+$//;
		$dir .= "/";
	}
	open(file, "<" . $file) ||
		usage("unable to open test vector information file '$file' for reading: $!\n");
	$vec = { desc => "", file => "", sha256 => "", sha384 => "", sha512 => "" };
	$data = $field = "";
	$line = 0;
	while(<file>) {
		$line++;
		s/\s*[\r\n]+$//;
		next if ($field && $field ne "description" && !$_);
		if (/^(description|file|sha256|sha384|sha512):$/) {
			if ($field eq "description") {
				$vec->{desc} = $data;
			} elsif ($field eq "file") {
				$data = $dir . $data if ($data !~ /^\//);
				$vec->{file} = $data;
			} elsif ($field eq "sha256") {
				$vec->{sha256} = $data;
			} elsif ($field eq "sha384") {
				$vec->{sha384} = $data;
			} elsif ($field eq "sha512") {
				$vec->{sha512} = $data;
			}
			$data = "";
			$field = $1;
		} elsif ($field eq "description") {
			s/^    //;
			$data .= $_ . "\n";
		} elsif ($field =~ /^sha\d\d\d$/) {
			s/^\s+//;
			if (!/^([a-f0-9]{32}|[a-f0-9]{64})$/) {
				usage("invalid sha-256/384/512 test vector information " .
				      "file format at line $line of file '$file'\n");
			}
			$data .= $_;
		} elsif ($field eq "file") {
			s/^    //;
			$data .= $_;
		} else {
			usage("invalid sha-256/384/512 test vector information file " .
			      "format at line $line of file '$file'\n");
		}
	}
	if ($field eq "description") {
		$data = $dir . $data if ($data !~ /^\//);
		$vec->{desc} = $data;
	} elsif ($field eq "file") {
		$vec->{file} = $data;
	} elsif ($field eq "sha256") {
		$vec->{sha256} = $data;
	} elsif ($field eq "sha384") {
		$vec->{sha384} = $data;
	} elsif ($field eq "sha512") {
		$vec->{sha512} = $data;
	} else {
		usage("invalid sha-256/384/512 test vector information file " .
		      "format.  missing required fields in file '$file'\n");
	}

	# sanity check all entries:
	if (!$vec->{desc}) {
		usage("invalid sha-256/384/512 test vector information file " .
		      "format.  missing required description field in file '$file'\n");
	}
	if (!$vec->{file}) {
		usage("invalid sha-256/384/512 test vector information file " .
		      "format.  missing required file field in file '$file'\n");
	}
	if (! -f $vec->{file}) {
		usage("the test vector data file (field file) name " .
		      "'$vec->{file}' is not a readable file.  check the file filed in " .
		      "file '$file'.\n");
	}
	if (!($vec->{sha256} || $vec->{sha384} || $vec->{sha512})) {
		usage("invalid sha-256/384/512 test vector information file " .
		      "format.  there must be at least one sha256, sha384, or sha512 " .
		      "field specified in file '$file'.\n");
	}
	if ($vec->{sha256} !~ /^(|[a-f0-9]{64})$/) {
		usage("invalid sha-256/384/512 test vector information file " .
		      "format.  the sha256 field is invalid in file '$file'.\n");
	}
	if ($vec->{sha384} !~ /^(|[a-f0-9]{96})$/) {
		usage("invalid sha-256/384/512 test vector information file " .
		      "format.  the sha384 field is invalid in file '$file'.\n");
	}
	if ($vec->{sha512} !~ /^(|[a-f0-9]{128})$/) {
		usage("invalid sha-256/384/512 test vector information file " .
		      "format.  the sha512 field is invalid in file '$file'.\n");
	}
	close(file);
	if ($hashes & (($vec->{sha256} ? 1 : 0) | ($vec->{sha384} ? 2 : 0) | ($vec->{sha512} ? 4 : 0))) {
		push(@vectors, $vec);
	}
}

usage("there were no test vectors for the specified hash(es) in any of the test vector information files you specified.\n") if (scalar(@vectors) < 1);

$num = $errors = $error256 = $error384 = $error512 = $tests = $test256 = $test384 = $test512 = 0;
foreach $vec (@vectors) {
	$num++;
	print "test vector #$num:\n";
	print "\t" . join("\n\t", split(/\n/, $vec->{desc})) . "\n";
	print "vector data file:\n\t$vec->{file}\n";
	$sha256 = $sha384 = $sha512 = "";
	if ($call) {
		$prog = $call;
		$prog =~ s/\%/'$vec->{file}'/g;
		@sha = grep(/[a-fa-f0-9]{64,128}/, split(/\n/, `$prog`));
		($sha256) = grep(/(^[a-fa-f0-9]{64}$|^[a-fa-f0-9]{64}[^a-fa-f0-9]|[^a-fa-f0-9][a-fa-f0-9]{64}$|[^a-fa-f0-9][a-fa-f0-9]{64}[^a-fa-f0-9])/, @sha);
		($sha384) = grep(/(^[a-fa-f0-9]{96}$|^[a-fa-f0-9]{96}[^a-fa-f0-9]|[^a-fa-f0-9][a-fa-f0-9]{96}$|[^a-fa-f0-9][a-fa-f0-9]{96}[^a-fa-f0-9])/, @sha);
		($sha512) = grep(/(^[a-fa-f0-9]{128}$|^[a-fa-f0-9]{128}[^a-fa-f0-9]|[^a-fa-f0-9][a-fa-f0-9]{128}$|[^a-fa-f0-9][a-fa-f0-9]{128}[^a-fa-f0-9])/, @sha);
	} else {
		if ($c256) {
			$prog = $c256;
			$prog =~ s/\%/'$vec->{file}'/g;
			@sha = grep(/[a-fa-f0-9]{64,128}/, split(/\n/, `$prog`));
			($sha256) = grep(/(^[a-fa-f0-9]{64}$|^[a-fa-f0-9]{64}[^a-fa-f0-9]|[^a-fa-f0-9][a-fa-f0-9]{64}$|[^a-fa-f0-9][a-fa-f0-9]{64}[^a-fa-f0-9])/, @sha);
		}
		if ($c384) {
			$prog = $c384;
			$prog =~ s/\%/'$vec->{file}'/g;
			@sha = grep(/[a-fa-f0-9]{64,128}/, split(/\n/, `$prog`));
			($sha384) = grep(/(^[a-fa-f0-9]{96}$|^[a-fa-f0-9]{96}[^a-fa-f0-9]|[^a-fa-f0-9][a-fa-f0-9]{96}$|[^a-fa-f0-9][a-fa-f0-9]{96}[^a-fa-f0-9])/, @sha);
		}
		if ($c512) {
			$prog = $c512;
			$prog =~ s/\%/'$vec->{file}'/g;
			@sha = grep(/[a-fa-f0-9]{64,128}/, split(/\n/, `$prog`));
			($sha512) = grep(/(^[a-fa-f0-9]{128}$|^[a-fa-f0-9]{128}[^a-fa-f0-9]|[^a-fa-f0-9][a-fa-f0-9]{128}$|[^a-fa-f0-9][a-fa-f0-9]{128}[^a-fa-f0-9])/, @sha);
		}
	}
	usage("unable to generate any hashes for file '$vec->{file}'!\n") if (!$sha256 && !$sha384 && $sha512);
	$sha256 =~ tr/a-f/a-f/;
	$sha384 =~ tr/a-f/a-f/;
	$sha512 =~ tr/a-f/a-f/;
	$sha256 =~ s/^.*([a-f0-9]{64}).*$/$1/;
	$sha384 =~ s/^.*([a-f0-9]{96}).*$/$1/;
	$sha512 =~ s/^.*([a-f0-9]{128}).*$/$1/;

	if ($sha256 && $hashes & 1 == 1) {
		if ($vec->{sha256} eq $sha256) {
			print "sha256 matches:\n\t$sha256\n"
		} else {
			print "sha256 does not match:\n\texpected:\n\t\t$vec->{sha256}\n" .
			      "\tgot:\n\t\t$sha256\n\n";
			$error256++;
		}
		$test256++;
	}
	if ($sha384 && $hashes & 2 == 2) {
		if ($vec->{sha384} eq $sha384) {
			print "sha384 matches:\n\t" . substr($sha384, 0, 64) . "\n\t" .
			      substr($sha384, -32) . "\n";
		} else {
			print "sha384 does not match:\n\texpected:\n\t\t" .
			      substr($vec->{sha384}, 0, 64) . "\n\t\t" .
			      substr($vec->{sha384}, -32) . "\n\tgot:\n\t\t" .
			      substr($sha384, 0, 64) . "\n\t\t" . substr($sha384, -32) . "\n\n";
			$error384++;
		}
		$test384++;
	}
	if ($sha512 && $hashes & 4 == 4) {
		if ($vec->{sha512} eq $sha512) {
			print "sha512 matches:\n\t" . substr($sha512, 0, 64) . "\n\t" .
			      substr($sha512, -64) . "\n";
		} else {
			print "sha512 does not match:\n\texpected:\n\t\t" .
			      substr($vec->{sha512}, 0, 64) . "\n\t\t" .
			      substr($vec->{sha512}, -32) . "\n\tgot:\n\t\t" .
			      substr($sha512, 0, 64) . "\n\t\t" . substr($sha512, -64) . "\n\n";
			$error512++;
		}
		$test512++;
	}
}

$errors = $error256 + $error384 + $error512;
$tests = $test256 + $test384 + $test512;
print "\n\n===== results ($num vector data files hashed) =====\n\n";
print "hash type\tno. of tests\tpassed\tfailed\n";
print "---------\t------------\t------\t------\n";
if ($test256) {
	$pass = $test256 - $error256;
	print "sha-256\t\t".substr("           $test256", -12)."\t".substr("     $pass", -6)."\t".substr("     $error256", -6)."\n";
}
if ($test384) {
	$pass = $test384 - $error384;
	print "sha-384\t\t".substr("           $test384", -12)."\t".substr("     $pass", -6)."\t".substr("     $error384", -6)."\n";
}
if ($test512) {
	$pass = $test512 - $error512;
	print "sha-512\t\t".substr("           $test512", -12)."\t".substr("     $pass", -6)."\t".substr("     $error512", -6)."\n";
}
print "----------------------------------------------\n";
$pass = $tests - $errors;
print "total:          ".substr("           $tests", -12)."\t".substr("     $pass", -6)."\t".substr("     $errors", -6)."\n\n";
print "no errors!  all tests were successful!\n\n" if (!$errors);

