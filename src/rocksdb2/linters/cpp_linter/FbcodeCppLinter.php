<?php

class fbcodecpplinter extends arcanistlinter {
  const cpplint      = "/home/engshare/tools/cpplint";
  const lint_error   = 1;
  const lint_warning = 2;
  const c_flag = "--c_mode=true";
  private $rawlintoutput = array();

  public function willlintpaths(array $paths) {
    $futures = array();
    $ret_value = 0;
    $last_line = system("which cpplint", $ret_value);
    $cpp_lint = false;
    if ($ret_value == 0) {
      $cpp_lint = $last_line;
    } else if (file_exists(self::cpplint)) {
      $cpp_lint = self::cpplint;
    }

    if ($cpp_lint) {
      foreach ($paths as $p) {
        $lpath = $this->getengine()->getfilepathondisk($p);
        $lpath_file = file($lpath);
        if (preg_match('/\.(c)$/', $lpath) ||
            preg_match('/-\*-.*mode: c[; ].*-\*-/', $lpath_file[0]) ||
            preg_match('/vim(:.*)*:\s*(set\s+)?filetype=c\s*:/', $lpath_file[0])
            ) {
          $futures[$p] = new execfuture("%s %s %s 2>&1",
                             $cpp_lint, self::c_flag,
                             $this->getengine()->getfilepathondisk($p));
        } else {
          $futures[$p] = new execfuture("%s %s 2>&1",
            self::cpplint, $this->getengine()->getfilepathondisk($p));
        }
      }

      foreach (futures($futures)->limit(8) as $p => $f) {
        $this->rawlintoutput[$p] = $f->resolvex();
      }
    }
    return;
  }

  public function getlintername() {
    return "fbcpp";
  }

  public function lintpath($path) {
    $msgs = $this->getcpplintoutput($path);
    foreach ($msgs as $m) {
      $this->raiselintatline($m['line'], 0, $m['severity'], $m['msg']);
    }
  }

  public function getlintseveritymap() {
    return array(
      self::lint_warning => arcanistlintseverity::severity_warning,
      self::lint_error   => arcanistlintseverity::severity_error
    );
  }

  public function getlintnamemap() {
    return array(
      self::lint_warning => "cpplint warning",
      self::lint_error   => "cpplint error"
    );
  }

  private function getcpplintoutput($path) {
    list($output) = $this->rawlintoutput[$path];

    $msgs = array();
    $current = null;
    foreach (explode("\n", $output) as $line) {
      if (preg_match('/[^:]*\((\d+)\):(.*)$/', $line, $matches)) {
        if ($current) {
          $msgs[] = $current;
        }
        $line = $matches[1];
        $text = $matches[2];
        $sev  = preg_match('/.*warning.*/', $text)
                  ? self::lint_warning
                  : self::lint_error;
        $current = array('line'     => $line,
                         'msg'      => $text,
                         'severity' => $sev);
      } else if ($current) {
        $current['msg'] .= ' ' . $line;
      }
    }
    if ($current) {
      $msgs[] = $current;
    }

    return $msgs;
  }
}

