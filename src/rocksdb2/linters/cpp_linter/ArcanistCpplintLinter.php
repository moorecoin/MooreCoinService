<?php

/**
 * uses google's cpplint.py to check code. rocksdb team forked this file from
 * phabricator's /src/lint/linter/arcanistcpplintlinter.php, and customized it
 * for its own use.
 *
 * you can get it here:
 * http://google-styleguide.googlecode.com/svn/trunk/cpplint/cpplint.py
 * @group linter
 */
final class arcanistcpplintlinter extends arcanistlinter {

  public function willlintpaths(array $paths) {
    return;
  }

  public function getlintername() {
    return 'cpplint.py';
  }

  public function getlintpath() {
    $bin = 'cpplint.py';
    // search under current dir
    list($err) = exec_manual('which %s/%s', $this->linterdir(), $bin);
    if (!$err) {
      return $this->linterdir().'/'.$bin;
    }

    // look for globally installed cpplint.py
    list($err) = exec_manual('which %s', $bin);
    if ($err) {
      throw new arcanistusageexception(
        "cpplint.py does not appear to be installed on this system. install ".
        "it (e.g., with 'wget \"http://google-styleguide.googlecode.com/".
        "svn/trunk/cpplint/cpplint.py\"') ".
        "in your .arcconfig to point to the directory where it resides. ".
        "also don't forget to chmod a+x cpplint.py!");
    }

    return $bin;
  }

  public function lintpath($path) {
    $bin = $this->getlintpath();
    $path = $this->rocksdbdir().'/'.$path;

    $f = new execfuture("%c $path", $bin);

    list($err, $stdout, $stderr) = $f->resolve();

    if ($err === 2) {
      throw new exception("cpplint failed to run correctly:\n".$stderr);
    }

    $lines = explode("\n", $stderr);
    $messages = array();
    foreach ($lines as $line) {
      $line = trim($line);
      $matches = null;
      $regex = '/^[^:]+:(\d+):\s*(.*)\s*\[(.*)\] \[(\d+)\]$/';
      if (!preg_match($regex, $line, $matches)) {
        continue;
      }
      foreach ($matches as $key => $match) {
        $matches[$key] = trim($match);
      }
      $message = new arcanistlintmessage();
      $message->setpath($path);
      $message->setline($matches[1]);
      $message->setcode($matches[3]);
      $message->setname($matches[3]);
      $message->setdescription($matches[2]);
      $message->setseverity(arcanistlintseverity::severity_warning);
      $this->addlintmessage($message);
    }
  }

  // the path of this linter
  private function linterdir() {
    return dirname(__file__);
  }

  // todo(kaili) a quick and dirty way to figure out rocksdb's root dir.
  private function rocksdbdir() {
    return $this->linterdir()."/../..";
  }
}
