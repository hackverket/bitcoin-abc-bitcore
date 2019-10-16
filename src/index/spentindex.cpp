// Copyright (c) 2017-2018 The Bitcoin Core developers
// Copyright (c) 2019 The Bitcore ABC developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <index/spentindex.h>

#include <chain.h>
#include <init.h>
#include <ui_interface.h>
#include <util/system.h>
#include <validation.h>
#include <index/indexutil.h>

#include <boost/thread.hpp>

static const char DB_SPENTINDEX = 'p';

std::unique_ptr<SpentIndex> g_spentindex;

class SpentIndex::DB : public BaseIndex::DB {
public:
    explicit DB(size_t n_cache_size, bool f_memory = false,
                bool f_wipe = false);
    bool ReadSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value) {
        return Read(std::make_pair(DB_SPENTINDEX, key), value);
    }

    bool UpdateSpentIndex(const std::vector<std::pair<CSpentIndexKey, CSpentIndexValue> >&vect) {
        CDBBatch batch(*this);
        for (std::vector<std::pair<CSpentIndexKey,CSpentIndexValue> >::const_iterator it=vect.begin(); it!=vect.end(); it++) {
            if (it->second.IsNull()) {
                batch.Erase(std::make_pair(DB_SPENTINDEX, it->first));
            } else {
                batch.Write(std::make_pair(DB_SPENTINDEX, it->first), it->second);
            }
        }
        return WriteBatch(batch);
    }
};

SpentIndex::DB::DB(size_t n_cache_size, bool f_memory, bool f_wipe)
    : BaseIndex::DB(GetDataDir() / "indexes" / "spentindex", n_cache_size,
                    f_memory, f_wipe) {}


SpentIndex::SpentIndex(size_t n_cache_size, bool f_memory, bool f_wipe)
    : m_db(std::make_unique<SpentIndex::DB>(n_cache_size, f_memory, f_wipe)) {}

SpentIndex::~SpentIndex() {}

void SpentIndex::SpendCoins(
    const size_t indexInBlock,
    const CTransaction &tx,
    const CBlockIndex *pindex,
    const CCoinsViewCache& view)
{
    if (tx.IsCoinBase()) {
        return;
    }
    const uint256 txhash = tx.GetHash();
    for (size_t j = 0; j < tx.vin.size(); j++) {
        const CTxIn input = tx.vin[j];
        const CTxOut &prevout = view.GetOutputFor(tx.vin[j]);
        uint160 hashBytes;
        int addressType;

        std::tie(hashBytes, addressType) = GetHashAndAddressType(prevout);

        // add the spent index to determine the txid and input that spent an output
        // and to find the amount and address from an input
        spentIndex.push_back(std::make_pair(CSpentIndexKey(input.prevout.GetTxId(), input.prevout.GetN()), CSpentIndexValue(txhash, j, pindex->nHeight, prevout.nValue / SATOSHI, addressType, hashBytes)));
    }
}

bool SpentIndex::WriteChanges() {
    bool ok = m_db->UpdateSpentIndex(spentIndex);
    spentIndex.clear();
	if (!ok) {
        return AbortNode("Failed to write spent index.");
	}
    return ok;
}

void SpentIndex::UndoCoinSpend(const CTxIn& input) {

    // TODO: This is dead code, as changes are never written.
    // See GitHub issue #4
    return;

    // undo and delete the spent index
    spentIndex.push_back(std::make_pair(
                CSpentIndexKey(input.prevout.GetTxId(), input.prevout.GetN()),
                CSpentIndexValue()));
}

BaseIndex::DB &SpentIndex::GetDB() const {
    return *m_db;
}

bool SpentIndex::ReadSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value) {
    return m_db->ReadSpentIndex(key, value);
}
