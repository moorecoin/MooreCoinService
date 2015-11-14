# ripple_app

## ledgermaster.cpp

- change getledgerbyhash() to not use "all bits zero" to mean
  "return the current ledger"

- replace uint32 with ledgerindex and choose appropriate names

## ihashrouter.h

- rename to hashrouter.h

## hashrouter.cpp

- rename hashrouter to hashrouterimp

- inline functions

- comment appropriately

- determine the semantics of the uint256, replace it with an appropriate
  typedef like ripplepublickey or whatever is appropriate.

- provide good symbolic names for the config tunables.

## beast

- change stoppable to not require a constructor with parameters
