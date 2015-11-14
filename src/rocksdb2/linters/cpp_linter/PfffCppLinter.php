<?php
// copyright 2004-present moorecoin.  all rights reserved.

class pfffcpplinter extends arcanistlinter {
  const program      = "/home/engshare/tools/checkcpp";

  public function getlintername() {
    return "checkcpp";
  }
  public function getlintnamemap() {
    return array(
    );
  }

  public function getlintseveritymap() {
    return array(
    );
  }

  public function willlintpaths(array $paths) {
    $program = false;
    $ret_value = 0;
    $last_line = system("which checkcpp", $ret_value);
    if ($ret_value == 0) {
      $program = $last_line;
    } else if (file_exists(self::program)) {
      $program = self::program;
    }
    if ($program) {
      $futures = array();
      foreach ($paths as $p) {
        $futures[$p] = new execfuture("%s --lint %s 2>&1",
          $program, $this->getengine()->getfilepathondisk($p));
      }
      foreach (futures($futures)->limit(8) as $p => $f) {

        list($stdout, $stderr) = $f->resolvex();
        $raw = json_decode($stdout, true);
        if (!is_array($raw)) {
          throw new exception(
            "checkcpp returned invalid json!".
            "stdout: {$stdout} stderr: {$stderr}"
          );
        }
        foreach($raw as $err) {
          $this->addlintmessage(
            arcanistlintmessage::newfromdictionary(
              array(
                'path' => $err['file'],
                'line' => $err['line'],
                'char' => 0,
                'name' => $err['name'],
                'description' => $err['info'],
                'code' => $this->getlintername(),
                'severity' => arcanistlintseverity::severity_warning,
              )
            )
          );
        }
      }
    }
    return;
  }

  public function lintpath($path) {
    return;
  }
}
