from test_framework.messages import ToHex
from test_framework.util import assert_equal

def check_validaterawtx(node, tx,
     has_valid_inputs = True,
     is_minable = True,
     enough_fee = True, num_errors = 0):

    res = node.validaterawtransaction(ToHex(tx))

    if enough_fee:
        assert(res["txfee"] >= res["txfeeneeded"])
    else:
        assert(res["txfee"] <= res["txfeeneeded"])

    assert_equal(res["minable"], is_minable)
    assert_equal(res["inputscheck"]["valid"], has_valid_inputs)
    assert_equal(num_errors, len(res["errors"]))
