# Bitcore ABC

This is a patchset on top of [Bitcoin ABC](https://www.bitcoinabc.org) adding
additional indexing functionality.

This software is experimental. See issues for known issues.

## License

Bitcoin ABC is released under the terms of the MIT license. See
[COPYING](COPYING) for more information or see
https://opensource.org/licenses/MIT.

# Notable release notes

## v0.20.4 - Oct 2019

Upstream removed `CBitcoinAddress` and friends, so there are fair amount of
changes with how addresses are encoded and decoded in RPC calls.

**NOTE: RPC calls that would previously output legacy addresses, now output
cashaddr. To disable this, you can globally disable cashaddr with the configuration
setting `usecashaddr=0`**

## v0.19.7 - Oct 2019

**Indexes refactored to new architecture**

With ABC 0.19.7 a new txindex architecture was introduced. Bitcores additional
indexes were also using the same database, so these need to be migrated.

The timestamp index is fully migrated to the new architecture. This means it
can be turned on and off without doing a full reindex. It will sync
asynchronously at its own pace in separate thread. It is now located at
`<datadir>/indexes/timestamp`.

The address index is partially migrated. It is now located at
`<datadir>/indexes/addressindex`. It is still indexed as part of block
synchronization and turning it off or on requires a full reindex.

The spent index is also partially migrade. It is located at
`<datadir>/indexes/spentindex`.

Only partial migration of these two indexes is possible, as the new indexing
architecture is not powerful enough. In particular, it is not possible to access
the utxo state at random point in history.

**NOTE: There is no data migration, or data cleanup for old databases. You need
to do a full reindexing when upgrading.**

Most of the indexing code has now refactored from `validation.cpp` to
`src/index/[spentindex|addressindex|timestampindex].cpp`.
