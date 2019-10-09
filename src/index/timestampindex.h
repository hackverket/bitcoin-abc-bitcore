// Copyright (c) 2017-2018 The Bitcoin Core developers
// Copyright (c) 2019 The Bitcore ABC developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INDEX_TIMESTAMPINDEX_H
#define BITCOIN_INDEX_TIMESTAMPINDEX_H

#include <index/base.h>
#include <txdb.h>

class TimestampIndex final : public BaseIndex {
protected:
    class DB;

private:
    const std::unique_ptr<DB> m_db;

protected:
    bool WriteBlock(const CBlock &block, const CBlockIndex *pindex) override;

    BaseIndex::DB &GetDB() const override;

    const char *GetName() const override { return "spentindex"; }

public:
    explicit TimestampIndex(size_t n_cache_size, bool f_memory = false,
                     bool f_wipe = false);

    virtual ~TimestampIndex() override;

    bool GetTimestampIndex(const unsigned int &high, const unsigned int &low, const bool fActiveOnly, std::vector<std::pair<uint256, unsigned int> > &hashes);
};

extern std::unique_ptr<TimestampIndex> g_timestampindex;
#endif
