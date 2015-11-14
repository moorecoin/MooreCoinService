package org.rocksdb.util;

public class environment {
  private static string os = system.getproperty("os.name").tolowercase();

  public static boolean iswindows() {
    return (os.indexof("win") >= 0);
  }

  public static boolean ismac() {
    return (os.indexof("mac") >= 0);
  }

  public static boolean isunix() {
    return (os.indexof("nix") >= 0 ||
            os.indexof("nux") >= 0 ||
            os.indexof("aix") >= 0);
  }

  public static string getsharedlibraryname(string name) {
    if (isunix()) {
      return string.format("lib%s.so", name);
    } else if (ismac()) {
      return string.format("lib%s.dylib", name);
    }
    throw new unsupportedoperationexception();
  }

  public static string getjnilibraryname(string name) {
    if (isunix()) {
      return string.format("lib%s.so", name);
    } else if (ismac()) {
      return string.format("lib%s.jnilib", name);
    }
    throw new unsupportedoperationexception();
  }
}
