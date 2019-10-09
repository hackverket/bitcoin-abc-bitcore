// Copyright (c) 2017-2018 The Bitcoin Core developers
// Copyright (c) 2019 The Bitcore ABC developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INDEX_SPENTINDEX_H
#define BITCOIN_INDEX_SPENTINDEX_H

#include <index/base.h>
#include <txdb.h>

class SpentIndex final : public BaseIndex {
protected:
    class DB;

private:
    const std::unique_ptr<DB> m_db;

    // in memory buffer, comit with this->Write()
    std::vector<std::pair<CSpentIndexKey, CSpentIndexValue> > spentIndex;

protected:
    BaseIndex::DB &GetDB() const override;

    const char *GetName() const override { return "spentindex"; }

public:
    explicit SpentIndex(size_t n_cache_size, bool f_memory = false,
                     bool f_wipe = false);

	void SpendCoins(
			const size_t indexInBlock,
			const CTransaction &tx,
			const CBlockIndex *pindex,
			const CCoinsViewCache& view);

	void UndoCoinSpend(const CTxIn& input);


    virtual ~SpentIndex() override;

    bool ReadSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value);

	bool WriteChanges();
};

extern std::unique_ptr<SpentIndex> g_spentindex;
#endif
