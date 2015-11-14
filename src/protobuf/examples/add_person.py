#! /usr/bin/python

# see readme.txt for information and build instructions.

import addressbook_pb2
import sys

# this function fills in a person message based on user input.
def promptforaddress(person):
  person.id = int(raw_input("enter person id number: "))
  person.name = raw_input("enter name: ")

  email = raw_input("enter email address (blank for none): ")
  if email != "":
    person.email = email

  while true:
    number = raw_input("enter a phone number (or leave blank to finish): ")
    if number == "":
      break

    phone_number = person.phone.add()
    phone_number.number = number

    type = raw_input("is this a mobile, home, or work phone? ")
    if type == "mobile":
      phone_number.type = addressbook_pb2.person.mobile
    elif type == "home":
      phone_number.type = addressbook_pb2.person.home
    elif type == "work":
      phone_number.type = addressbook_pb2.person.work
    else:
      print "unknown phone type; leaving as default value."

# main procedure:  reads the entire address book from a file,
#   adds one person based on user input, then writes it back out to the same
#   file.
if len(sys.argv) != 2:
  print "usage:", sys.argv[0], "address_book_file"
  sys.exit(-1)

address_book = addressbook_pb2.addressbook()

# read the existing address book.
try:
  f = open(sys.argv[1], "rb")
  address_book.parsefromstring(f.read())
  f.close()
except ioerror:
  print sys.argv[1] + ": file not found.  creating a new file."

# add an address.
promptforaddress(address_book.person.add())

# write the new address book back to disk.
f = open(sys.argv[1], "wb")
f.write(address_book.serializetostring())
f.close()
