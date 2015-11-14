// see readme.txt for information and build instructions.

#include <iostream>
#include <fstream>
#include <string>
#include "addressbook.pb.h"
using namespace std;

// this function fills in a person message based on user input.
void promptforaddress(tutorial::person* person) {
  cout << "enter person id number: ";
  int id;
  cin >> id;
  person->set_id(id);
  cin.ignore(256, '\n');

  cout << "enter name: ";
  getline(cin, *person->mutable_name());

  cout << "enter email address (blank for none): ";
  string email;
  getline(cin, email);
  if (!email.empty()) {
    person->set_email(email);
  }

  while (true) {
    cout << "enter a phone number (or leave blank to finish): ";
    string number;
    getline(cin, number);
    if (number.empty()) {
      break;
    }

    tutorial::person::phonenumber* phone_number = person->add_phone();
    phone_number->set_number(number);

    cout << "is this a mobile, home, or work phone? ";
    string type;
    getline(cin, type);
    if (type == "mobile") {
      phone_number->set_type(tutorial::person::mobile);
    } else if (type == "home") {
      phone_number->set_type(tutorial::person::home);
    } else if (type == "work") {
      phone_number->set_type(tutorial::person::work);
    } else {
      cout << "unknown phone type.  using default." << endl;
    }
  }
}

// main function:  reads the entire address book from a file,
//   adds one person based on user input, then writes it back out to the same
//   file.
int main(int argc, char* argv[]) {
  // verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  google_protobuf_verify_version;

  if (argc != 2) {
    cerr << "usage:  " << argv[0] << " address_book_file" << endl;
    return -1;
  }

  tutorial::addressbook address_book;

  {
    // read the existing address book.
    fstream input(argv[1], ios::in | ios::binary);
    if (!input) {
      cout << argv[1] << ": file not found.  creating a new file." << endl;
    } else if (!address_book.parsefromistream(&input)) {
      cerr << "failed to parse address book." << endl;
      return -1;
    }
  }

  // add an address.
  promptforaddress(address_book.add_person());

  {
    // write the new address book back to disk.
    fstream output(argv[1], ios::out | ios::trunc | ios::binary);
    if (!address_book.serializetoostream(&output)) {
      cerr << "failed to write address book." << endl;
      return -1;
    }
  }

  // optional:  delete all global objects allocated by libprotobuf.
  google::protobuf::shutdownprotobuflibrary();

  return 0;
}
