#!/usr/bin/env python3
# Copyright (c) 2019
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.messages import COIN, CTransaction, CTxIn, CTxOut, COutPoint, ToHex, FromHex
from test_framework.script import *
from test_framework.test_framework import BitcoinTestFramework
from test_framework.txtools import pad_tx
from test_framework.util import assert_equal

def create_tx(node, undersize = False, low_fee = False, sign_tx = True):
    utxo = node.listunspent().pop()
    value = int(utxo["amount"]) * COIN

    if not low_fee:
        value -= 300

    addressHash = bytes([11,47,10,12,49,191,224,64,107,12,204,19,129,253,190,49,25,70,218,220])
    scriptPubKey = CScript([OP_DUP, OP_HASH160, addressHash, OP_EQUALVERIFY, OP_CHECKSIG])

    tx = CTransaction()
    tx.vin = [CTxIn(COutPoint(int(utxo["txid"], 16), utxo["vout"]))]
    tx.vout = [CTxOut(value, scriptPubKey)]

    if undersize:
        # Signing the tx bumps it over 100 bytes.
        assert(not sign_tx)
        assert(len(tx.serialize()) < 100)
    else:
        pad_tx(tx)

    tx.calc_sha256()
    if sign_tx:
        tx = FromHex(CTransaction(), node.signrawtransactionwithwallet(ToHex(tx))["hex"])

    return tx

def validate_tx(node, tx, has_valid_inputs = True, is_minable = True, enough_fee = True, num_errors = 0):
    res = node.validaterawtransaction(ToHex(tx))

    if enough_fee:
        assert(res["txfee"] >= res["txfeeneeded"])
    else:
        assert(res["txfee"] <= res["txfeeneeded"])

    assert_equal(res["minable"], is_minable)
    assert_equal(res["inputscheck"]["valid"], has_valid_inputs)
    assert_equal(num_errors, len(res["errors"]))

class ValidateRawTransactionTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [[ ]]

    def run_test(self):
        self.log.info("Test started");
        n = self.nodes[0]
        n.generate(105)
        validate_tx(n, create_tx(n))

        validate_tx(n, create_tx(n, low_fee = True),
            enough_fee = False)

        validate_tx(n, create_tx(n, sign_tx = False),
            has_valid_inputs = False,
            is_minable = False,
            num_errors = len(["input-script-failed"]))

        validate_tx(n, create_tx(n, sign_tx = False, undersize = True),
            has_valid_inputs = False,
            is_minable = False,
            num_errors = len(["input-script-failed", "bad-txns-undersize"]))

if __name__ == '__main__':
    ValidateRawTransactionTest().main()
