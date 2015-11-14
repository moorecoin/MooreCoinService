/*
	a custom randombytes must implement:

	void ed25519_fn(ed25519_randombytes_unsafe) (void *p, size_t len);

	ed25519_randombytes_unsafe is used by the batch verification function
	to create random scalars
*/
