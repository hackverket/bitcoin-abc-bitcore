// Copyright (c) 2017-2018 The Bitcoin Core developers
// Copyright (c) 2019 The Bitcore ABC developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INDEX_ADDRESSINDEX_H
#define BITCOIN_INDEX_ADDRESSINDEX_H

#include <index/base.h>
#include <txdb.h>

class CBlockUndo;

class AddressIndex final : public BaseIndex {
protected:
    class DB;

private:
    const std::unique_ptr<DB> m_db;

    // memory buffer, commit with this->WriteChanges()
    struct {
        std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
        std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > addressUnspentIndex;
    } writebuffer;
    struct {
        std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    } erasebuffer;

    void ClearBuffers() {
        writebuffer.addressIndex.clear();
        writebuffer.addressUnspentIndex.clear();
        erasebuffer.addressIndex.clear();
    }


public:

    /// Hopefully, we can call WriteBlock through BaseIndex in the future and
    /// run indexing in a different thread, but for now, the BaseIndex
    /// interface is not powerful enough.
    void AddCoins(
            const size_t indexInBlock,
            const CTransaction& tx,
            const CBlockIndex *pindex,
            const CCoinsViewCache& view);

    void SpendCoins(
            const size_t indexInBlock,
            const CTransaction& tx,
            const CBlockIndex *pindex,
            const CCoinsViewCache& view);

	void UndoCoinSpend(
			const size_t txInputIndex,
			const size_t txIndexInBlock,
			const CTransaction& tx,
			const Coin& undo,
			const CBlockIndex* pindex,
			const CCoinsViewCache& view);

	void UndoCoinAdd(
		const size_t txInputIndex,
		const size_t txIndexInBlock,
		const CTransaction& tx,
		const CBlockIndex* pindex);

    bool WriteChanges();

    BaseIndex::DB &GetDB() const override;

    const char *GetName() const override { return "addressindex"; }

    explicit AddressIndex(size_t n_cache_size, bool f_memory = false,
                     bool f_wipe = false);

    virtual ~AddressIndex() override;

    bool ReadAddressIndex(uint160 addressHash, int type,
            std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,
            int start, int end);

    bool ReadAddressUnspentIndex(uint160 addressHash, int type,
            std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs);

};

extern std::unique_ptr<AddressIndex> g_addressindex;
#endif
