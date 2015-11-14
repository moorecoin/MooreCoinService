
# m-of-n / multi-signature support on ripple

in order to enhance the flexibility of ripple and provide support for enhanced security of accounts, native support for "multi-signature" or "multisign" accounts is required.

transactions on an account which is designated as multisign can be authorized either by using the master or regular keys (unless those are disabled) or by being signed by a certain number (a quorum) of preauthorized accounts.

some technical details, including tables indicating some of the ripple commands and ledger entries to be used for implementing multi-signature, are currently listed on the [wiki](https://ripple.com/wiki/multisign) but will eventually be migrated into this document as well.

## steps to multisign

the implementation of multisign is a protocol breaking change which will require the coordinated rollout and deployment of the feature on the ripple network.

critical components for multisign are:

* ticket support
* authorized signer list management
* verification of multiple signatures during tx processing.

### ticket support

**associated jira task is [ripd-368](https://ripplelabs.atlassian.net/browse/ripd-368)**

currently transactions on the ripple network require the use of sequence numbers and sequence numbers must monotonically increase. since the sequence number is part of the transaction, it is "covered" by the signature that authorizes the transaction, which means that the sequence number would have to be decided at the time of transaction issuance. this would mean that multi-signature transactions could only be processed in strict "first-in, first-out" order which is not practical.

tickets can be used in lieu of sequence number. a ticket is a special token which, through a transaction, can be issued by any account and can be configured with an optional expiry date and an optional associated account.

#### specifics

the expiry date can be used to constrain the validity of the ticket. if specified, the ticket will be considered invalid and unusable if the closing  time of the last *validated* ledger is greater than or equal to the expiration time of the ticket.

the associated account can be used to specify an account, other than the issuing account, that is allowed to "consume" the ticket. consuming a ticket means to use the ticket in a transaction that is accepted by the network and makes its way into a validated ledger. if not present, the ticket can only be consumed by the issuing account.

*corner case:* it is possible that two or more transactions reference the same ticket and that both go into the same consensus set. during final application of transactions from the consensus set at most one of these transactions may succeed; others must fail with the indication that the ticket has been consumed.

*reserve:* while a ticket is outstanding, it should count against the reserve of the *issuer*.

##### issuance
we should decide whether, in the case of multi-signature accounts, any single authorized signer can issue a ticket on the multisignature accounts' behalf. this approach has both advantages and disadvantages.

advantages include:

+ simpler logic for tickets and reduced data - no need to store or consider an associated account.
+ owner reserves for issued tickets count against the multi-signature account instead of the account of the signer proposing a transaction.
+ cleaner meta-data: easier to follow who issued a ticket and how many tickets are outstanding and associated with a particular account.

disadvantages are:

+ any single authorized signer can issue multiple tickets, each counting against the account's reserve.
+ special-case logic for authorizing tickets on multi-sign accounts.

i believe that the disadvantages outweigh the advantages, but debate is welcome.

##### proposed transaction cancelation
a transaction that has been proposed against a multi-sign account using a ticket can be positively cancelled if a quorum of authorized signers sign and issue a transaction that consumes that ticket.

### authorized signer list management

for accounts which are designated as multi-sign, there must be a list which specifies which accounts are authorized to sign and the quorum threshold.

the quorum threshold indicates the minimum required weight that the sum of the weights of all signatures must have before a transaction can be authorized. it is an unsigned integer that is at least 2.

each authorized account can be given a weight, from 1 to 32 inclusive. the weight is used when to determine whether a given number of signatures are sufficient for a quorum.

### verification of multiple signatures during tx processing
the current approach to adding multi-signature support is to require that a transaction is to be signed outside the ripple network and only submitted after the quorum has been reached.

this reduces the implementation footprint and the load imposed on the network, and mirrors the way transaction signing is currently handled. it will require some messaging mechanism outside the ripple network to disseminate the proposed transaction to the authorized signers and to allow them apply signatures.

supporting in-ledger voting should be considered, but it has both advantages and disadvantages.

one of the advantages is increased transparency - transactions are visible as are the "votes" (the authorized accounts signing the transaction) on the ledger. however, transactions may also languish for a long time in the ledger, never reaching a quorum and consuming network resources.

### signature format
we should not develop a new format for multi-sign signatures. instead each signer should extract and sign the transaction as he normally would if this were a regular transaction. the resulting signature will be stored as a pair of { signing-account, signature } in an array of signatures associated with this transaction.

the advantage of this is that we can reuse the existing signing and verification code, and leverage the work that will go towards implementing support for the ed25519 elliptic curve.


### fees
multi-signature transactions impose a heavier load on the network and should claim higher fees.

the fee structure is not yet decided, but an escalating fee structure is laid out and discussed on the [wiki](https://ripple.com/wiki/multisign). this might need to be revisited and designed in light of discussions about changing how fees are handled.
