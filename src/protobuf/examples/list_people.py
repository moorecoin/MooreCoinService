#! /usr/bin/python

# see readme.txt for information and build instructions.

import addressbook_pb2
import sys

# iterates though all people in the addressbook and prints info about them.
def listpeople(address_book):
  for person in address_book.person:
    print "person id:", person.id
    print "  name:", person.name
    if person.hasfield('email'):
      print "  e-mail address:", person.email

    for phone_number in person.phone:
      if phone_number.type == addressbook_pb2.person.mobile:
        print "  mobile phone #:",
      elif phone_number.type == addressbook_pb2.person.home:
        print "  home phone #:",
      elif phone_number.type == addressbook_pb2.person.work:
        print "  work phone #:",
      print phone_number.number

# main procedure:  reads the entire address book from a file and prints all
#   the information inside.
if len(sys.argv) != 2:
  print "usage:", sys.argv[0], "address_book_file"
  sys.exit(-1)

address_book = addressbook_pb2.addressbook()

# read the existing address book.
f = open(sys.argv[1], "rb")
address_book.parsefromstring(f.read())
f.close()

listpeople(address_book)
