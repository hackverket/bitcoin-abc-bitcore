#!/usr/bin/env python3

from test_framework.test_framework import BitcoinTestFramework
from test_framework.messages import (
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    ToHex,
    FromHex)
from test_framework.script import (
    CScript,
    hash160,
    OP_EQUAL,
    OP_HASH160,
    OP_TRUE,
)
from test_framework.validaterawtransaction import check_validaterawtx
from test_framework.util import sync_mempools

# Copied from abc-segwit-recovery-activation
# Returns 2 transactions:
# 1) txfund: create outputs in segwit addresses
# 2) txspend: spends outputs from segwit addresses
def create_segwit_fund_and_spend_tx(node, spend, case0=False):
    if not case0:
        # To make sure we'll be able to recover coins sent to segwit addresses,
        # we test using historical recoveries from btc.com:
        # Spending from a P2SH-P2WPKH coin,
        #   txhash:a45698363249312f8d3d93676aa714be59b0bd758e62fa054fb1ea6218480691
        redeem_script0 = bytearray.fromhex(
            '0014fcf9969ce1c98a135ed293719721fb69f0b686cb')
        # Spending from a P2SH-P2WSH coin,
        #   txhash:6b536caf727ccd02c395a1d00b752098ec96e8ec46c96bee8582be6b5060fa2f
        redeem_script1 = bytearray.fromhex(
            '0020fc8b08ed636cb23afcb425ff260b3abd03380a2333b54cfa5d51ac52d803baf4')
    else:
        redeem_script0 = bytearray.fromhex('51020000')
        redeem_script1 = bytearray.fromhex('53020080')
    redeem_scripts = [redeem_script0, redeem_script1]

    # Fund transaction to segwit addresses
    txfund = CTransaction()
    txfund.vin = [CTxIn(COutPoint(int(spend['txid'], 16), spend['vout']))]
    amount = (50 * COIN - 1000) // len(redeem_scripts)
    for redeem_script in redeem_scripts:
        txfund.vout.append(
            CTxOut(amount, CScript([OP_HASH160, hash160(redeem_script), OP_EQUAL])))
    txfund = FromHex(CTransaction(), node.signrawtransactionwithwallet(ToHex(txfund))["hex"])
    txfund.rehash()

    # Segwit spending transaction
    # We'll test if a node that checks for standardness accepts this
    # txn. It should fail exclusively because of the restriction in
    # the scriptSig (non clean stack..), so all other characteristcs
    # must pass standardness checks. For this reason, we create
    # standard P2SH outputs.
    txspend = CTransaction()
    for i in range(len(redeem_scripts)):
        txspend.vin.append(
            CTxIn(COutPoint(txfund.sha256, i), CScript([redeem_scripts[i]])))
    txspend.vout = [CTxOut(50 * COIN - 2000,
                           CScript([OP_HASH160, hash160(CScript([OP_TRUE])), OP_EQUAL]))]
    txspend.rehash()

    return txfund, txspend



class ValidateRawSegwitTransactionTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            ["-acceptnonstdtxn=1"],
            ["-acceptnonstdtxn=0"]]

    def run_test(self):
        self.log.info("Test started");
        node_accepting_nonstandard = self.nodes[0]
        node_rejecting_nonstandard = self.nodes[1]
        n = node_rejecting_nonstandard
        n.generate(105)

        out = n.listunspent()

        txfund, txspend = create_segwit_fund_and_spend_tx(n, out.pop())
        txfund_case0, txspend_case0 = create_segwit_fund_and_spend_tx(n, out.pop(), True)

        # Funding transaction standard complient and is accepted by both nodes
        for tx in [txfund, txfund_case0]:
            check_validaterawtx(node_rejecting_nonstandard,
                    tx, num_errors = 0)
            check_validaterawtx(node_accepting_nonstandard,
                    tx, num_errors = 0)

        n.sendrawtransaction(ToHex(txfund))
        n.sendrawtransaction(ToHex(txfund_case0))
        sync_mempools(self.nodes)

        # Recovery transaction is non-standard
        for tx in [txspend, txspend_case0]:
            check_validaterawtx(node_rejecting_nonstandard,
                tx,
                is_minable = False,
                has_valid_inputs = False,
                num_errors = len(["input-script-failed"]))
#            # Upstream has changed to a weird policy for segwit transactions,
#            # they are no longer accepted to mempool, even though they are
#            # valid non-standard transactions.
#            check_validaterawtx(node_accepting_nonstandard,
#                tx, num_errors = 0)
#
#        node_accepting_nonstandard.sendrawtransaction(ToHex(txspend))
#        node_accepting_nonstandard.sendrawtransaction(ToHex(txspend_case0))
            check_validaterawtx(node_accepting_nonstandard,
                tx,
                is_minable = False,
                has_valid_inputs = False,
                num_errors = len(["input-script-failed"]))

if __name__ == '__main__':
    ValidateRawSegwitTransactionTest().main()


