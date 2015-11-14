// see readme.txt for information and build instructions.

import com.example.tutorial.addressbookprotos.addressbook;
import com.example.tutorial.addressbookprotos.person;
import java.io.fileinputstream;
import java.io.ioexception;
import java.io.printstream;

class listpeople {
  // iterates though all people in the addressbook and prints info about them.
  static void print(addressbook addressbook) {
    for (person person: addressbook.getpersonlist()) {
      system.out.println("person id: " + person.getid());
      system.out.println("  name: " + person.getname());
      if (person.hasemail()) {
        system.out.println("  e-mail address: " + person.getemail());
      }

      for (person.phonenumber phonenumber : person.getphonelist()) {
        switch (phonenumber.gettype()) {
          case mobile:
            system.out.print("  mobile phone #: ");
            break;
          case home:
            system.out.print("  home phone #: ");
            break;
          case work:
            system.out.print("  work phone #: ");
            break;
        }
        system.out.println(phonenumber.getnumber());
      }
    }
  }

  // main function:  reads the entire address book from a file and prints all
  //   the information inside.
  public static void main(string[] args) throws exception {
    if (args.length != 1) {
      system.err.println("usage:  listpeople address_book_file");
      system.exit(-1);
    }

    // read the existing address book.
    addressbook addressbook =
      addressbook.parsefrom(new fileinputstream(args[0]));

    print(addressbook);
  }
}
