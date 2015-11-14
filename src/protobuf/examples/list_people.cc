// see readme.txt for information and build instructions.

#include <iostream>
#include <fstream>
#include <string>
#include "addressbook.pb.h"
using namespace std;

// iterates though all people in the addressbook and prints info about them.
void listpeople(const tutorial::addressbook& address_book) {
  for (int i = 0; i < address_book.person_size(); i++) {
    const tutorial::person& person = address_book.person(i);

    cout << "person id: " << person.id() << endl;
    cout << "  name: " << person.name() << endl;
    if (person.has_email()) {
      cout << "  e-mail address: " << person.email() << endl;
    }

    for (int j = 0; j < person.phone_size(); j++) {
      const tutorial::person::phonenumber& phone_number = person.phone(j);

      switch (phone_number.type()) {
        case tutorial::person::mobile:
          cout << "  mobile phone #: ";
          break;
        case tutorial::person::home:
          cout << "  home phone #: ";
          break;
        case tutorial::person::work:
          cout << "  work phone #: ";
          break;
      }
      cout << phone_number.number() << endl;
    }
  }
}

// main function:  reads the entire address book from a file and prints all
//   the information inside.
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
    if (!address_book.parsefromistream(&input)) {
      cerr << "failed to parse address book." << endl;
      return -1;
    }
  }

  listpeople(address_book);

  // optional:  delete all global objects allocated by libprotobuf.
  google::protobuf::shutdownprotobuflibrary();

  return 0;
}
