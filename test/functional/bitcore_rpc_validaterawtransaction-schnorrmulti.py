#!/usr/bin/env python3

from test_framework.test_framework import BitcoinTestFramework
from test_framework import schnorr
from test_framework.validaterawtransaction import check_validaterawtx
from test_framework.blocktools import create_transaction
from test_framework.script import (
    CScript,
    OP_0,
    OP_1,
    OP_2,
    OP_CHECKMULTISIG,
    OP_TRUE,
    SIGHASH_ALL,
    SIGHASH_FORKID,
    SignatureHashForkId,
)
from test_framework.key import CECKey
from test_framework.messages import (
    FromHex,
    CTransaction,
    ToHex,
    CTxOut,
    CTxIn,
    COutPoint)
##
# Verify that validaterawtransaction handles schnorr multisig transactions.

# From 'abc-schnorrmultisig-activation'
def create_fund_and_spend_tx(node, spendfrom, dummy):

    privkeybytes = b"Schnorr!" * 4
    private_key = CECKey()
    private_key.set_secretbytes(privkeybytes)
    # get uncompressed public key serialization
    public_key = private_key.get_pubkey()

    script = CScript([OP_1, public_key, OP_1, OP_CHECKMULTISIG])

    value = spendfrom.vout[0].nValue
    value -= 1000

    # Fund transaction
    txfund = create_transaction(spendfrom, 0, b'', value, script)
    txfund = FromHex(CTransaction(), node.signrawtransactionwithwallet(ToHex(txfund))["hex"])
    txfund.rehash()
    #fundings.append(txfund)

    # Spend transaction
    txspend = CTransaction()
    txspend.vout.append(
        CTxOut(value-1000, CScript([OP_TRUE])))
    txspend.vin.append(
        CTxIn(COutPoint(txfund.sha256, 0), b''))

    # Sign the transaction
    sighashtype = SIGHASH_ALL | SIGHASH_FORKID
    hashbyte = bytes([sighashtype & 0xff])
    sighash = SignatureHashForkId(
        script, txspend, 0, sighashtype, value)
    txsig = schnorr.sign(privkeybytes, sighash) + hashbyte
    txspend.vin[0].scriptSig = CScript([dummy, txsig])
    txspend.rehash()

    return txfund, txspend

# the upgrade activation time, which we artificially set far into the future
GRAVITON_START_TIME = 2000000000

# If we don't do this, autoreplay protection will activate before graviton and
# all our sigs will mysteriously fail.
REPLAY_PROTECTION_START_TIME = GRAVITON_START_TIME * 2


class ValidateRawSchnorrMultiTransactionTest(BitcoinTestFramework):

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

        txfund1, txspend1 = create_fund_and_spend_tx(
                n, spent_as_tx(out.pop()), dummy = OP_1)
        txfund2, txspend2_invalid = create_fund_and_spend_tx(
                n, spent_as_tx(out.pop()), dummy = OP_0)
        txfund3, txspend3_invalid = create_fund_and_spend_tx(
                n, spent_as_tx(out.pop()), dummy = OP_2)

        for tx in [txfund1, txfund2, txfund3]:
            check_validaterawtx(n, tx, num_errors = 0)
            n.sendrawtransaction(ToHex(tx))
            assert tx.hash in n.getrawmempool()

        check_validaterawtx(n, txspend1, num_errors = 0)
        check_validaterawtx(n, txspend2_invalid,
            is_minable = False,
            has_valid_inputs = False,
            num_errors = 1)
        check_validaterawtx(n, txspend3_invalid,
            is_minable = False,
            has_valid_inputs = False,
            num_errors = 1)

        n.sendrawtransaction(ToHex(txspend1))
        assert txspend1.hash in n.getrawmempool()

if __name__ == '__main__':
    ValidateRawSchnorrMultiTransactionTest().main()


