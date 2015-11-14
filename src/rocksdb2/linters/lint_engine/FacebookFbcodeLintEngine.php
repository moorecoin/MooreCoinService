<?php
// copyright 2004-present moorecoin.  all rights reserved.

class moorecoinfbcodelintengine extends arcanistlintengine {

  public function buildlinters() {
    $linters = array();
    $paths = $this->getpaths();

    // remove all deleted files, which are not checked by the
    // following linters.
    foreach ($paths as $key => $path) {
      if (!filesystem::pathexists($this->getfilepathondisk($path))) {
        unset($paths[$key]);
      }
    }

    $generated_linter = new arcanistgeneratedlinter();
    $linters[] = $generated_linter;

    $nolint_linter = new arcanistnolintlinter();
    $linters[] = $nolint_linter;

    $text_linter = new arcanisttextlinter();
    $text_linter->setcustomseveritymap(array(
      arcanisttextlinter::lint_line_wrap
        => arcanistlintseverity::severity_advice,
    ));
    $linters[] = $text_linter;

    $java_text_linter = new arcanisttextlinter();
    $java_text_linter->setmaxlinelength(100);
    $java_text_linter->setcustomseveritymap(array(
      arcanisttextlinter::lint_line_wrap
        => arcanistlintseverity::severity_advice,
    ));
    $linters[] = $java_text_linter;

    $pep8_options = $this->getpep8withtextoptions().',e302';

    $python_linter = new arcanistpep8linter();
    $python_linter->setconfig(array('options' => $pep8_options));
    $linters[] = $python_linter;

    $python_2space_linter = new arcanistpep8linter();
    $python_2space_linter->setconfig(array('options' => $pep8_options.',e111'));
    $linters[] = $python_2space_linter;

   // currently we can't run cpplint in commit hook mode, because it
    // depends on having access to the working directory.
    if (!$this->getcommithookmode()) {
      $cpp_linters = array();
      $google_linter = new arcanistcpplintlinter();
      $google_linter->setconfig(array(
        'lint.cpplint.prefix' => '',
        'lint.cpplint.bin' => 'cpplint',
      ));
      $cpp_linters[] = $linters[] = $google_linter;
      $cpp_linters[] = $linters[] = new fbcodecpplinter();
      $cpp_linters[] = $linters[] = new pfffcpplinter();
    }

    $spelling_linter = new arcanistspellinglinter();
    $linters[] = $spelling_linter;

    foreach ($paths as $path) {
      $is_text = false;

      $text_extensions = (
        '/\.('.
        'cpp|cxx|c|cc|h|hpp|hxx|tcc|'.
        'py|rb|hs|pl|pm|tw|'.
        'php|phpt|css|js|'.
        'java|'.
        'thrift|'.
        'lua|'.
        'siv|'.
        'txt'.
        ')$/'
      );
      if (preg_match($text_extensions, $path)) {
        $is_text = true;
      }
      if ($is_text) {
        $nolint_linter->addpath($path);

        $generated_linter->addpath($path);
        $generated_linter->adddata($path, $this->loaddata($path));

        if (preg_match('/\.java$/', $path)) {
          $java_text_linter->addpath($path);
          $java_text_linter->adddata($path, $this->loaddata($path));
        } else {
          $text_linter->addpath($path);
          $text_linter->adddata($path, $this->loaddata($path));
        }

        $spelling_linter->addpath($path);
        $spelling_linter->adddata($path, $this->loaddata($path));
      }
      if (preg_match('/\.(cpp|c|cc|cxx|h|hh|hpp|hxx|tcc)$/', $path)) {
        foreach ($cpp_linters as &$linter) {
          $linter->addpath($path);
          $linter->adddata($path, $this->loaddata($path));
        }
      }

      // match *.py and contbuild config files
      if (preg_match('/(\.(py|tw|smcprops)|^contbuild\/configs\/[^\/]*)$/',
                    $path)) {
        $space_count = 4;
        $real_path = $this->getfilepathondisk($path);
        $dir = dirname($real_path);
        do {
          if (file_exists($dir.'/.python2space')) {
            $space_count = 2;
            break;
          }
          $dir = dirname($dir);
        } while ($dir != '/' && $dir != '.');

        if ($space_count == 4) {
          $cur_path_linter = $python_linter;
        } else {
          $cur_path_linter = $python_2space_linter;
        }
        $cur_path_linter->addpath($path);
        $cur_path_linter->adddata($path, $this->loaddata($path));

        if (preg_match('/\.tw$/', $path)) {
          $cur_path_linter->setcustomseveritymap(array(
            'e251' => arcanistlintseverity::severity_disabled,
          ));
        }
      }
    }

    $name_linter = new arcanistfilenamelinter();
    $linters[] = $name_linter;
    foreach ($paths as $path) {
      $name_linter->addpath($path);
    }

    return $linters;
  }

}
