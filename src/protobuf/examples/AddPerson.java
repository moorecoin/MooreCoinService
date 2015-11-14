// see readme.txt for information and build instructions.

import com.example.tutorial.addressbookprotos.addressbook;
import com.example.tutorial.addressbookprotos.person;
import java.io.bufferedreader;
import java.io.fileinputstream;
import java.io.filenotfoundexception;
import java.io.fileoutputstream;
import java.io.inputstreamreader;
import java.io.ioexception;
import java.io.printstream;

class addperson {
  // this function fills in a person message based on user input.
  static person promptforaddress(bufferedreader stdin,
                                 printstream stdout) throws ioexception {
    person.builder person = person.newbuilder();

    stdout.print("enter person id: ");
    person.setid(integer.valueof(stdin.readline()));

    stdout.print("enter name: ");
    person.setname(stdin.readline());

    stdout.print("enter email address (blank for none): ");
    string email = stdin.readline();
    if (email.length() > 0) {
      person.setemail(email);
    }

    while (true) {
      stdout.print("enter a phone number (or leave blank to finish): ");
      string number = stdin.readline();
      if (number.length() == 0) {
        break;
      }

      person.phonenumber.builder phonenumber =
        person.phonenumber.newbuilder().setnumber(number);

      stdout.print("is this a mobile, home, or work phone? ");
      string type = stdin.readline();
      if (type.equals("mobile")) {
        phonenumber.settype(person.phonetype.mobile);
      } else if (type.equals("home")) {
        phonenumber.settype(person.phonetype.home);
      } else if (type.equals("work")) {
        phonenumber.settype(person.phonetype.work);
      } else {
        stdout.println("unknown phone type.  using default.");
      }

      person.addphone(phonenumber);
    }

    return person.build();
  }

  // main function:  reads the entire address book from a file,
  //   adds one person based on user input, then writes it back out to the same
  //   file.
  public static void main(string[] args) throws exception {
    if (args.length != 1) {
      system.err.println("usage:  addperson address_book_file");
      system.exit(-1);
    }

    addressbook.builder addressbook = addressbook.newbuilder();

    // read the existing address book.
    try {
      fileinputstream input = new fileinputstream(args[0]);
      try {
        addressbook.mergefrom(input);
      } finally {
        try { input.close(); } catch (throwable ignore) {}
      }
    } catch (filenotfoundexception e) {
      system.out.println(args[0] + ": file not found.  creating a new file.");
    }

    // add an address.
    addressbook.addperson(
      promptforaddress(new bufferedreader(new inputstreamreader(system.in)),
                       system.out));

    // write the new address book back to disk.
    fileoutputstream output = new fileoutputstream(args[0]);
    try {
      addressbook.build().writeto(output);
    } finally {
      output.close();
    }
  }
}
