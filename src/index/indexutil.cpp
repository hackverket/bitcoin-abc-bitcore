// Copyright (c) 2019 The Bitcore ABC developers

#include <index/indexutil.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <boost/variant/static_visitor.hpp>

std::pair<uint160, int> GetHashAndAddressType(const CTxOut& prevout) {
    if (prevout.scriptPubKey.IsPayToScriptHash()) {
        uint160 hash(std::vector<unsigned char>(
                    prevout.scriptPubKey.begin()+2,
                    prevout.scriptPubKey.begin()+22));
        return { hash, ADDRESSTYPE_P2SH };
    }

    if (prevout.scriptPubKey.IsPayToPublicKeyHash()) {
        uint160 hash(std::vector <unsigned char>(
                    prevout.scriptPubKey.begin()+3,
                    prevout.scriptPubKey.begin()+23));
        return { hash, ADDRESSTYPE_P2PKH };
    }
    uint160 hash;
    hash.SetNull();
    return { hash, ADDRESSTYPE_UNKNOWN };
}


// Decode hash and address type from CTxDestination
class AddressDecoder : public boost::static_visitor<std::pair<uint160, int>> {
public:
    std::pair<uint160, int> operator()(const CKeyID &id) const {
        std::vector<uint8_t> data;
        data.insert(data.end(), std::begin(id), std::end(id));
        return { uint160(data), ADDRESSTYPE_P2PKH };
    }

    std::pair<uint160, int> operator()(const CScriptID &id) const {
        std::vector<uint8_t> data;
        data.insert(data.end(), std::begin(id), std::end(id));
        return { uint160(data), ADDRESSTYPE_P2SH };
    }

    std::pair<uint160, int> operator()(const CNoDestination &) const {
        uint160 hash;
        hash.SetNull();
        return { hash, ADDRESSTYPE_UNKNOWN };
    }
};

std::pair<uint160, int> GetHashAndAddressType(const CTxDestination& dst) {
    return boost::apply_visitor(AddressDecoder{}, dst);
}
