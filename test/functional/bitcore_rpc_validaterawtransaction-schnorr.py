#!/usr/bin/env python3

from test_framework.test_framework import BitcoinTestFramework
from test_framework import schnorr
from test_framework.validaterawtransaction import check_validaterawtx
from test_framework.blocktools import create_transaction
from test_framework.script import (
    CScript,
    OP_1,
    OP_CHECKMULTISIG,
    OP_CHECKSIG,
    OP_TRUE,
    SIGHASH_ALL,
    SIGHASH_FORKID,
    SignatureHashForkId,
)
from test_framework.key import CECKey
from test_framework.messages import (
    COIN,
    FromHex,
    CTransaction,
    ToHex,
    CTxOut,
    CTxIn,
    COutPoint)
##
# Verify that validaterawtransaction handles schnorr signed transactions.

# From 'abc-schnorr-activation'
def create_fund_and_spend_tx(node, spend, multi=False):

    privkeybytes = b"Schnorr!" * 4
    private_key = CECKey()
    private_key.set_secretbytes(privkeybytes)
    # get uncompressed public key serialization
    public_key = private_key.get_pubkey()

    if multi:
        script = CScript([OP_1, public_key, OP_1, OP_CHECKMULTISIG])
    else:
        script = CScript([public_key, OP_CHECKSIG])

    # Fund transaction
    prevtx = FromHex(CTransaction(), node.getrawtransaction(spend['txid']))
    prevtx.rehash()
    fee = 500
    fund_amount = 50 * COIN - fee
    txfund = create_transaction(
        prevtx, spend['vout'], b'', fund_amount, script)
    txfund = FromHex(CTransaction(), node.signrawtransactionwithwallet(ToHex(txfund))["hex"])
    txfund.rehash()

    # Spend transaction
    txspend = CTransaction()
    txspend.vout.append(
        CTxOut(fund_amount - 1000, CScript([OP_TRUE])))
    txspend.vin.append(
        CTxIn(COutPoint(txfund.sha256, 0), b''))

    # Sign the transaction
    sighashtype = SIGHASH_ALL | SIGHASH_FORKID
    hashbyte = bytes([sighashtype & 0xff])
    sighash = SignatureHashForkId(
        script, txspend, 0, sighashtype, fund_amount)
    txsig = schnorr.sign(privkeybytes, sighash) + hashbyte
    if multi:
        txspend.vin[0].scriptSig = CScript([b'', txsig])
    else:
        txspend.vin[0].scriptSig = CScript([txsig])
    txspend.rehash()

    return txfund, txspend

class ValidateRawSchnorrTransactionTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [
            ["-acceptnonstdtxn=1"]]

    def run_test(self):
        self.log.info("Test started");
        n = self.nodes[0]
        n.generate(105)

        out = n.listunspent()

        txfund, txspend = create_fund_and_spend_tx(n, out.pop())
        txfund_multi, txspend_multi = create_fund_and_spend_tx(n, out.pop(), multi = True)

        for tx in [txfund, txfund_multi]:
            check_validaterawtx(n, tx, num_errors = 0)
            n.sendrawtransaction(ToHex(tx))
            assert tx.hash in n.getrawmempool()

        # non-mutli sig schnorr is OK
        check_validaterawtx(n, txspend, num_errors = 0)
        # mutli sig schnorr is not allowed
        check_validaterawtx(n, txspend_multi,
            is_minable = False,
            has_valid_inputs = False,
            num_errors = len(['input-script-failed']))

        n.sendrawtransaction(ToHex(txspend))
        assert tx.hash in n.getrawmempool()

if __name__ == '__main__':
    ValidateRawSchnorrTransactionTest().main()


