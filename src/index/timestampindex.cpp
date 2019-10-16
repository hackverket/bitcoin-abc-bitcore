// Copyright (c) 2017-2018 The Bitcoin Core developers
// Copyright (c) 2019 The Bitcore ABC developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <index/timestampindex.h>

#include <chain.h>
#include <init.h>
#include <ui_interface.h>
#include <util/system.h>
#include <validation.h>

#include <boost/thread.hpp>

static const char DB_TIMESTAMPINDEX = 's';
static const char DB_BLOCKHASHINDEX = 'z';

std::unique_ptr<TimestampIndex> g_timestampindex;

class TimestampIndex::DB : public BaseIndex::DB {
public:
    explicit DB(size_t n_cache_size, bool f_memory = false,
                bool f_wipe = false);

    bool WriteTimestampIndex(const CTimestampIndexKey &timestampIndex) {
        CDBBatch batch(*this);
        batch.Write(std::make_pair(DB_TIMESTAMPINDEX, timestampIndex), 0);
        return WriteBatch(batch);
    }

    bool ReadTimestampIndex(const unsigned int &high, const unsigned int &low, const bool fActiveOnly, std::vector<std::pair<uint256, unsigned int> > &hashes) {

        boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

        pcursor->Seek(std::make_pair(DB_TIMESTAMPINDEX, CTimestampIndexIteratorKey(low)));

        while (pcursor->Valid()) {
            boost::this_thread::interruption_point();
            std::pair<char, CTimestampIndexKey> key;
            if (pcursor->GetKey(key) && key.first == DB_TIMESTAMPINDEX && key.second.timestamp < high) {
                if (fActiveOnly) {
                    if (HashOnchainActive(key.second.blockHash)) {
                        hashes.push_back(std::make_pair(key.second.blockHash, key.second.timestamp));
                    }
                } else {
                    hashes.push_back(std::make_pair(key.second.blockHash, key.second.timestamp));
                }

                pcursor->Next();
            } else {
                break;
            }
        }

        return true;
    }

    bool WriteTimestampBlockIndex(const CTimestampBlockIndexKey &blockhashIndex, const CTimestampBlockIndexValue &logicalts) {
        CDBBatch batch(*this);
        batch.Write(std::make_pair(DB_BLOCKHASHINDEX, blockhashIndex), logicalts);
        return WriteBatch(batch);
    }

    bool ReadTimestampBlockIndex(const uint256 &hash, unsigned int &ltimestamp) {

        CTimestampBlockIndexValue lts;
        if (!Read(std::make_pair(DB_BLOCKHASHINDEX, hash), lts))
        return false;

        ltimestamp = lts.ltimestamp;
        return true;
    }

};

TimestampIndex::DB::DB(size_t n_cache_size, bool f_memory, bool f_wipe)
    : BaseIndex::DB(GetDataDir() / "indexes" / "timestamp", n_cache_size,
                    f_memory, f_wipe) {}


TimestampIndex::TimestampIndex(size_t n_cache_size, bool f_memory, bool f_wipe)
    : m_db(std::make_unique<TimestampIndex::DB>(n_cache_size, f_memory, f_wipe)) {}

TimestampIndex::~TimestampIndex() {}

bool TimestampIndex::WriteBlock(const CBlock &block, const CBlockIndex *pindex)
{
    unsigned int logicalTS = pindex->nTime;
    unsigned int prevLogicalTS = 0;

    // retrieve logical timestamp of the previous block
    if (pindex->pprev)
        if (!m_db->ReadTimestampBlockIndex(pindex->pprev->GetBlockHash(), prevLogicalTS))
            LogPrintf("%s: Failed to read previous block's logical timestamp\n", __func__);

    if (logicalTS <= prevLogicalTS) {
        logicalTS = prevLogicalTS + 1;
        LogPrintf("%s: Previous logical timestamp is newer Actual[%d] prevLogical[%d] Logical[%d]\n", __func__, pindex->nTime, prevLogicalTS, logicalTS);
    }

    if (!m_db->WriteTimestampIndex(CTimestampIndexKey(logicalTS, pindex->GetBlockHash()))) {
        LogPrintf("*** %s\n", "Failed to write timestamp index");
        return false;
    }
    if (!m_db->WriteTimestampBlockIndex(CTimestampBlockIndexKey(pindex->GetBlockHash()), CTimestampBlockIndexValue(logicalTS))) {
        LogPrintf("*** %s\n", "Failed to write blockhash index");
        return false;
    }
    return true;
}

BaseIndex::DB &TimestampIndex::GetDB() const {
    return *m_db;
}

bool TimestampIndex::GetTimestampIndex(
    const unsigned int &high,
    const unsigned int &low,
    const bool fActiveOnly,
    std::vector<std::pair<uint256, unsigned int> > &hashes)
{
    return m_db->ReadTimestampIndex(high, low, fActiveOnly, hashes);
}
