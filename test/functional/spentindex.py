#!/usr/bin/env python3
# Copyright (c) 2014-2015 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test addressindex generation and fetching
#

import time
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from test_framework.script import *
from test_framework.mininode import *
from test_framework.blocktools import *
from test_framework.key import CECKey
from test_framework.messages import COIN, ToHex, FromHex

TX_FEE = 10000
MEMPOOL_HEIGHT = -1

class SpentIndexTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        self.node1_args = ["-debug", "-spentindex", "-usecashaddr=0", "-persistmempool=0"]
        self.extra_args = [
            ["-debug"],
            self.node1_args,
            ["-debug", "-spentindex"],
            ["-debug", "-spentindex", "-txindex", "-usecashaddr=0"]
        ]

    def setup_network(self):
        self.setup_nodes()

        # Nodes 0/1 are "wallet" nodes
        # Nodes 2/3 are used for testing
        connect_nodes(self.nodes[0], self.nodes[1])
        connect_nodes(self.nodes[0], self.nodes[2])
        connect_nodes(self.nodes[0], self.nodes[3])

        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        self.nodes[0].generate(105)
        self.sync_all()

        chain_height = self.nodes[1].getblockcount()
        assert_equal(chain_height, 105)
        self.test_basics()
        self.test_reorg()

    def test_reorg(self):
        self.log.info("Reorg test")
        utxo = self.nodes[0].listunspent().pop()

        spentinfo_args = {
            'txid' : utxo['txid'],
            'index' :  utxo['vout']
        }

        # Not spent yet, should fail
        assert_raises_rpc_error(-5, "Unable to get spent info",
                self.nodes[1].getspentinfo, spentinfo_args)

        # Spend utxo to mempool
        prevtx = FromHex(CTransaction(), self.nodes[0].getrawtransaction(utxo['txid']))
        prevtx.calc_sha256()
        tx = create_transaction(
            prevtx = prevtx, n = utxo['vout'],
            sig = b'', value = int(utxo['amount'] * COIN - TX_FEE))
        tx_hex = self.nodes[0].signrawtransactionwithwallet(ToHex(tx))['hex']
        tx = FromHex(CTransaction(), tx_hex)
        tx.calc_sha256()
        self.nodes[0].sendrawtransaction(tx_hex)
        self.sync_all()

        info = self.nodes[1].getspentinfo(spentinfo_args)
        assert_equal(MEMPOOL_HEIGHT, info['height'])
        assert_equal(0, info['index'])
        assert_equal(tx.get_id(), info['txid'])

        # Confirm spend in a block
        self.nodes[0].generate(1)
        self.sync_all()
        tip_height = self.nodes[0].getblockcount()
        info = self.nodes[1].getspentinfo(spentinfo_args)
        assert_equal(tip_height, info['height'])
        assert_equal(0, info['index'])
        assert_equal(tx.get_id(), info['txid'])

        # Invalidate tip, should go back to mempool
        self.log.info("Test returning spend to mempool...")
        self.nodes[1].invalidateblock(self.nodes[1].getbestblockhash());
        info = self.nodes[1].getspentinfo(spentinfo_args)
        assert_equal(MEMPOOL_HEIGHT, info['height'])

        # Restart to clear the mempool, the spent info should not longer exist,
        # and call should fail.
        self.log.info("Test erasing a spend by reorg...")
        self.stop_node(1)
        self.start_node(1, self.node1_args)
        assert_raises_rpc_error(-5, "Unable to get spent info",
                self.nodes[1].getspentinfo, spentinfo_args)

        # Finally, mine longer chain to re-org the other nodes. Let another
        # mine the transaction again and observe it's re-written to index with
        # updated height.
        self.log.info("Test rewriting spend at different height...")
        connect_nodes(self.nodes[1], self.nodes[0])
        self.nodes[1].generate(100)
        sync_blocks(self.nodes)

        assert(1 == len(self.nodes[0].getrawmempool()))
        self.nodes[0].generate(1)
        assert(0 == len(self.nodes[0].getrawmempool()))
        sync_blocks(self.nodes)
        tip_height = self.nodes[1].getblockcount()
        info = self.nodes[1].getspentinfo(spentinfo_args)
        assert_equal(tip_height, info['height'])


    def test_basics(self):

        # Check that
        self.log.info("Testing spent index...")
        privkey = "cSdkPxkAjA4HDr5VHgsebAPDEh9Gyub4HK8UJr2DFGGqKKy4K5sG"
        address = "mgY65WSfEmsyYaYPQaXhmXMeBhwp4EcsQW"
        addressHash = bytes([11,47,10,12,49,191,224,64,107,12,204,19,129,253,190,49,25,70,218,220])
        scriptPubKey = CScript([OP_DUP, OP_HASH160, addressHash, OP_EQUALVERIFY, OP_CHECKSIG])
        unspent = self.nodes[0].listunspent()
        tx = CTransaction()
        amount = int(unspent[0]["amount"] * COIN - TX_FEE)
        tx.vin = [CTxIn(COutPoint(int(unspent[0]["txid"], 16), unspent[0]["vout"]))]
        tx.vout = [CTxOut(amount, scriptPubKey)]
        tx.rehash()

        signed_tx = self.nodes[0].signrawtransactionwithwallet(ToHex(tx))
        txid = self.nodes[0].sendrawtransaction(signed_tx["hex"], True)
        self.nodes[0].generate(1)
        self.sync_all()

        self.log.info("Testing getspentinfo method...")

        # Check that the spentinfo works standalone
        info = self.nodes[1].getspentinfo({
            "txid": unspent[0]["txid"],
            "index": unspent[0]["vout"]})
        assert_equal(info["txid"], txid)
        assert_equal(info["index"], 0)
        assert_equal(info["height"], 106)

        self.log.info("Testing getrawtransaction method...")

        # Check that verbose raw transaction includes spent info
        txVerbose = self.nodes[3].getrawtransaction(unspent[0]["txid"], 1)
        assert_equal(txVerbose["vout"][unspent[0]["vout"]]["spentTxId"], txid)
        assert_equal(txVerbose["vout"][unspent[0]["vout"]]["spentIndex"], 0)
        assert_equal(txVerbose["vout"][unspent[0]["vout"]]["spentHeight"], 106)

        # Check that verbose raw transaction includes input values
        txVerbose2 = self.nodes[3].getrawtransaction(txid, 1)
        assert_equal(float(txVerbose2["vin"][0]["value"]), (amount + TX_FEE) / COIN)
        assert_equal(txVerbose2["vin"][0]["valueSat"], amount + TX_FEE)

        # Check that verbose raw transaction includes address values and input values
        privkey2 = "cSdkPxkAjA4HDr5VHgsebAPDEh9Gyub4HK8UJr2DFGGqKKy4K5sG"
        address2 = "mgY65WSfEmsyYaYPQaXhmXMeBhwp4EcsQW"
        addressHash2 = bytes([11,47,10,12,49,191,224,64,107,12,204,19,129,253,190,49,25,70,218,220])
        scriptPubKey2 = CScript([OP_DUP, OP_HASH160, addressHash2, OP_EQUALVERIFY, OP_CHECKSIG])
        tx2 = CTransaction()
        tx2.vin = [CTxIn(COutPoint(int(txid, 16), 0))]
        amount = int(amount - TX_FEE);
        tx2.vout = [CTxOut(amount, scriptPubKey2)]
        tx.rehash()
        self.nodes[0].importprivkey(privkey)
        signed_tx2 = self.nodes[0].signrawtransactionwithwallet(ToHex(tx2))
        txid2 = self.nodes[0].sendrawtransaction(signed_tx2["hex"], True)

        # Check the mempool index
        self.sync_all()
        txVerbose3 = self.nodes[1].getrawtransaction(txid2, 1)
        assert_equal(txVerbose3["vin"][0]["address"], address2)
        assert_equal(txVerbose3["vin"][0]["valueSat"], amount + TX_FEE)
        assert_equal(float(txVerbose3["vin"][0]["value"]), (amount + TX_FEE) / COIN)


        # Check the database index
        block_hash = self.nodes[0].generate(1)
        self.sync_all()

        txVerbose4 = self.nodes[3].getrawtransaction(txid2, 1)
        assert_equal(txVerbose4["vin"][0]["address"], address2)
        assert_equal(txVerbose4["vin"][0]["valueSat"], amount + TX_FEE)
        assert_equal(float(txVerbose4["vin"][0]["value"]), (amount + TX_FEE) / COIN)

        # Check block deltas
        self.log.info("Testing getblockdeltas...")

        block = self.nodes[3].getblockdeltas(block_hash[0])
        assert_equal(len(block["deltas"]), 2)
        assert_equal(block["deltas"][0]["index"], 0)
        assert_equal(len(block["deltas"][0]["inputs"]), 0)
        assert_equal(len(block["deltas"][0]["outputs"]), 0)
        assert_equal(block["deltas"][1]["index"], 1)
        assert_equal(block["deltas"][1]["txid"], txid2)
        assert_equal(block["deltas"][1]["inputs"][0]["index"], 0)
        assert_equal(block["deltas"][1]["inputs"][0]["address"], "mgY65WSfEmsyYaYPQaXhmXMeBhwp4EcsQW")
        assert_equal(block["deltas"][1]["inputs"][0]["satoshis"], (amount + TX_FEE) * -1)
        assert_equal(block["deltas"][1]["inputs"][0]["prevtxid"], txid)
        assert_equal(block["deltas"][1]["inputs"][0]["prevout"], 0)
        assert_equal(block["deltas"][1]["outputs"][0]["index"], 0)
        assert_equal(block["deltas"][1]["outputs"][0]["address"], "mgY65WSfEmsyYaYPQaXhmXMeBhwp4EcsQW")
        assert_equal(block["deltas"][1]["outputs"][0]["satoshis"], amount)


if __name__ == '__main__':
    SpentIndexTest().main()
