# beast::asio

wrappers and utilities to make working with boost::asio easier.

## rules for asynchronous objects

if an object calls asynchronous initiating functions it must either:

	1. manage its lifetime by being reference counted

	or

	2. wait for all pending completion handlers to be called before
	   allowing itself to be destroyed.
