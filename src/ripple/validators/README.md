# validators

the validators module has these responsibilities:

- provide an administrative interface for maintaining the list _source_
  locations.
- report performance statistics on _source_ locations
- report performance statistics on _validators_ provided by _source_ locations.
- choose a suitable random subset of observed _validators_ to become the
  _chosen validators_ set.
- update the _chosen validators_ set as needed to meet performance requirements.

## description

the consensus process used by the ripple payment protocol requires that ledger
hashes be signed by _validators_, producing a _validation_. the integrity of
the process is mathematically assured when each node chooses a random subset
of _validators_ to trust, where each _validator_ is a public verifiable entity
that is independent. or more specifically, no entity should be in control of
any significant number of _validators_ chosen by each node.

the list of _validators_ a node chooses to trust is called the _chosen
validators_. the **validators** module implements business logic to automate the
selection of _chosen validators_ by allowing the administrator to provide one
or more trusted _sources_, from which _validators_ are learned. performance
statistics are tracked for these _validators_, and the module chooses a
suitable subset from which to form the _chosen validators_ list.

the module looks for these criteria to determine suitability:

- different validators are not controlled by the same entity.
- each validator participates in a majority of ledgers.
- a validator does not sign ledgers that fail the consensus process.

## terms

<table>
<tr>
  <td>chosen validators</td>
  <td>a set of validators chosen by the validators module. this is the new term
      for what was formerly known as the unique node list.
  </td>
</tr>
<tr>
  <td>source</td>
  <td>a trusted source of validator descriptors. examples: the rippled
      configuration file, a local text file,  or a trusted url such
      as https://ripple.com/validators.txt.
  </td></tr>
</tr>
<tr>
  <td>validation</td>
  <td>a closed ledger hash signed by a validator.
  </td>
</tr>
<tr>
  <td>validator</td>
  <td>a publicly verifiable entity which signs ledger hashes with its private
      key, and makes its public key available through out of band means.
  </td>
</tr>
</table>
