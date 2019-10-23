#!/usr/bin/env python3

from test_framework.test_framework import BitcoinTestFramework
from test_framework.validaterawtransaction import check_validaterawtx
from test_framework.blocktools import create_transaction
from test_framework.txtools import pad_tx
from test_framework.script import (
    CScript,
    OP_ADD,
    OP_TRUE,
    OP_1
)
from test_framework.messages import (
    FromHex,
    CTransaction,
    ToHex,
    CTxOut,
    CTxIn,
    COutPoint)

# From 'abc-minimaldata-activation'
def create_fund_and_spend_tx(node, spendfrom):
    script = CScript([OP_ADD])

    value = spendfrom.vout[0].nValue
    value -= 1000

    # Fund transaction
    txfund = create_transaction(spendfrom, 0, b'', value, script)
    txfund = FromHex(CTransaction(), node.signrawtransactionwithwallet(ToHex(txfund))["hex"])
    txfund.calc_sha256()

    # Spend transaction
    txspend = CTransaction()
    txspend.vout.append(
        CTxOut(value-1000, CScript([OP_TRUE])))
    txspend.vin.append(
        CTxIn(COutPoint(txfund.sha256, 0), b''))

    # Sign the transaction
    txspend.vin[0].scriptSig = CScript(
        b'\x01\x01\x51')  # PUSH1(0x01) OP_1
    pad_tx(txspend)
    txspend.rehash()

    return txfund, txspend



# the upgrade activation time, which we artificially set far into the future
GRAVITON_START_TIME = 2000000000

# If we don't do this, autoreplay protection will activate before graviton and
# all our sigs will mysteriously fail.
REPLAY_PROTECTION_START_TIME = GRAVITON_START_TIME * 2


class ValidateRawMinimaldata(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-gravitonactivationtime={}".format(
            GRAVITON_START_TIME),
            "-replayprotectionactivationtime={}".format(
            REPLAY_PROTECTION_START_TIME)]]

    def run_test(self):
        self.log.info("Test started");
        n = self.nodes[0]
        for i in range(105):
            n.setmocktime(GRAVITON_START_TIME + i)
            n.generate(1)

        out = n.listunspent()
        def spent_as_tx(out):
            txhex = n.getrawtransaction(out['txid'])
            tx = FromHex(CTransaction(), txhex)
            tx.calc_sha256()
            return tx

        txfund, invalid_txspend = create_fund_and_spend_tx(n, spent_as_tx(out.pop()))

        check_validaterawtx(n, txfund, num_errors = 0)
        n.sendrawtransaction(ToHex(txfund))
        assert txfund.hash in n.getrawmempool()

        check_validaterawtx(n, invalid_txspend,
            has_valid_inputs = False,
            is_minable = False,
            enough_fee = True,
            num_errors = 1)

        # Encoding the spends scriptSig with minimal encoding should work.
        txspend = invalid_txspend
        txspend.vin[0].scriptSig = CScript([OP_1, OP_1])
        pad_tx(txspend)
        check_validaterawtx(n, txspend, num_errors = 0)
        n.sendrawtransaction(ToHex(txspend))

if __name__ == '__main__':
    ValidateRawMinimaldata().main()


