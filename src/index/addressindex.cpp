// Copyright (c) 2017-2018 The Bitcoin Core developers
// Copyright (c) 2019 The Bitcore ABC developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <index/addressindex.h>
#include <index/indexutil.h>

#include <chain.h>
#include <init.h>
#include <ui_interface.h>
#include <util/system.h>
#include <validation.h>
#include <coins.h>
#include <undo.h>
#include <hash.h>

#include <boost/thread.hpp>

constexpr char DB_ADDRESSINDEX = 'a';
constexpr char DB_ADDRESSUNSPENTINDEX = 'u';

std::unique_ptr<AddressIndex> g_addressindex;

class AddressIndex::DB : public BaseIndex::DB {
public:
    explicit DB(size_t n_cache_size, bool f_memory = false,
                bool f_wipe = false);

    bool WriteAddressIndex(
        const std::vector<std::pair<CAddressIndexKey, CAmount > >&vect)
    {
        CDBBatch batch(*this);
        for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
            batch.Write(std::make_pair(DB_ADDRESSINDEX, it->first), it->second);
        return WriteBatch(batch);
    }

    bool EraseAddressIndex(const std::vector<std::pair<CAddressIndexKey, CAmount > >&vect) {
        CDBBatch batch(*this);
        for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
            batch.Erase(std::make_pair(DB_ADDRESSINDEX, it->first));
        return WriteBatch(batch);
    }

    bool ReadAddressIndex(uint160 addressHash, int type,
                                        std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,
                                        int start, int end) {

        boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

        if (start > 0 && end > 0) {
            pcursor->Seek(std::make_pair(DB_ADDRESSINDEX, CAddressIndexIteratorHeightKey(type, addressHash, start)));
        } else {
            pcursor->Seek(std::make_pair(DB_ADDRESSINDEX, CAddressIndexIteratorKey(type, addressHash)));
        }

        while (pcursor->Valid()) {
            boost::this_thread::interruption_point();
            std::pair<char,CAddressIndexKey> key;
            if (pcursor->GetKey(key) && key.first == DB_ADDRESSINDEX && key.second.hashBytes == addressHash) {
                if (end > 0 && key.second.blockHeight > end) {
                    break;
                }
                CAmount nValue;
                if (pcursor->GetValue(nValue)) {
                    addressIndex.push_back(std::make_pair(key.second, nValue));
                    pcursor->Next();
                } else {
                    return error("failed to get address index value");
                }
            } else {
                break;
            }
        }
        return true;
    }

    bool UpdateAddressUnspentIndex(const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&vect) {
        CDBBatch batch(*this);
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=vect.begin(); it!=vect.end(); it++) {
            if (it->second.IsNull()) {
                batch.Erase(std::make_pair(DB_ADDRESSUNSPENTINDEX, it->first));
            } else {
                batch.Write(std::make_pair(DB_ADDRESSUNSPENTINDEX, it->first), it->second);
            }
        }
        return WriteBatch(batch);
    }

    bool ReadAddressUnspentIndex(uint160 addressHash, int type,
                                               std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs) {

        boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

        pcursor->Seek(std::make_pair(DB_ADDRESSUNSPENTINDEX, CAddressIndexIteratorKey(type, addressHash)));

        while (pcursor->Valid()) {
            boost::this_thread::interruption_point();
            std::pair<char,CAddressUnspentKey> key;
            if (pcursor->GetKey(key) && key.first == DB_ADDRESSUNSPENTINDEX && key.second.hashBytes == addressHash) {
                CAddressUnspentValue nValue;
                if (pcursor->GetValue(nValue)) {
                    unspentOutputs.push_back(std::make_pair(key.second, nValue));
                    pcursor->Next();
                } else {
                    return error("failed to get address unspent value");
                }
            } else {
                break;
            }
        }

        return true;
    }

};

AddressIndex::DB::DB(size_t n_cache_size, bool f_memory, bool f_wipe)
    : BaseIndex::DB(GetDataDir() / "indexes" / "addressindex", n_cache_size,
                    f_memory, f_wipe) {}



AddressIndex::AddressIndex(size_t n_cache_size, bool f_memory, bool f_wipe)
    : m_db(std::make_unique<AddressIndex::DB>(n_cache_size, f_memory, f_wipe)) {}

AddressIndex::~AddressIndex() {}

void AddressIndex::AddCoins(
    const size_t indexInBlock,
    const CTransaction &tx,
    const CBlockIndex *pindex,
    const CCoinsViewCache& view)
{
    const uint256 txhash = tx.GetHash();

    for (unsigned int k = 0; k < tx.vout.size(); k++) {
        const CTxOut &out = tx.vout[k];

        if (out.scriptPubKey.IsPayToScriptHash()) {
            std::vector<unsigned char> hashBytes(
                out.scriptPubKey.begin()+2, out.scriptPubKey.begin()+22);

            // record receiving activity
            writebuffer.addressIndex.push_back(std::make_pair(
                        CAddressIndexKey(2, uint160(hashBytes),
                            pindex->nHeight, indexInBlock, txhash, k, false),
                        out.nValue / SATOSHI));

            // record unspent output
            writebuffer.addressUnspentIndex.push_back(std::make_pair(
                        CAddressUnspentKey(2, uint160(hashBytes), txhash, k),
                        CAddressUnspentValue(out.nValue / SATOSHI, out.scriptPubKey,
                            pindex->nHeight)));
            continue;

        }
        if (out.scriptPubKey.IsPayToPublicKeyHash()) {
            std::vector<unsigned char> hashBytes(
                    out.scriptPubKey.begin()+3, out.scriptPubKey.begin()+23);

            // record receiving activity
            writebuffer.addressIndex.push_back(std::make_pair(
                        CAddressIndexKey(1, uint160(hashBytes),
                            pindex->nHeight, indexInBlock, txhash, k, false),
                        out.nValue / SATOSHI));

            // record unspent output
            writebuffer.addressUnspentIndex.push_back(std::make_pair(
                        CAddressUnspentKey(1, uint160(hashBytes), txhash, k),
                        CAddressUnspentValue(out.nValue / SATOSHI,
                            out.scriptPubKey, pindex->nHeight)));

            continue;
        }
    }
}

void AddressIndex::SpendCoins(
	const size_t indexInBlock,
	const CTransaction& tx,
	const CBlockIndex* pindex,
	const CCoinsViewCache& view)
{
    const uint256 txhash = tx.GetHash();
	for (size_t j = 0; j < tx.vin.size(); j++) {
		const CTxIn input = tx.vin[j];
		const CTxOut &prevout = view.GetOutputFor(tx.vin[j]);
		uint160 hashBytes;
		int addressType;

        std::tie(hashBytes, addressType) = GetHashAndAddressType(prevout);

        if (addressType == ADDRESSTYPE_UNKNOWN) {
            continue;
        }

        // record spending activity
        writebuffer.addressIndex.push_back(std::make_pair(
                    CAddressIndexKey(addressType, hashBytes, pindex->nHeight,
                        indexInBlock, txhash, j, true),
                    prevout.nValue / SATOSHI * -1));

        // remove address from unspent index
        writebuffer.addressUnspentIndex.push_back(std::make_pair(
                    CAddressUnspentKey(addressType, hashBytes,
                        input.prevout.GetTxId(), input.prevout.GetN()),
                    CAddressUnspentValue()));
    }
}

void AddressIndex::UndoCoinSpend(
	const size_t txInputIndex,
    const size_t txIndexInBlock,
	const CTransaction& tx,
    const Coin& undo,
	const CBlockIndex* pindex,
    const CCoinsViewCache& view)
{
	const CTxOut &prevout = view.GetOutputFor(tx.vin[txInputIndex]);
	const TxId& txid = tx.GetId();
	const CTxIn& input = tx.vin[txInputIndex];

    uint160 hash;
    int addressType;
    std::tie(hash, addressType) = GetHashAndAddressType(prevout);

    if (addressType == ADDRESSTYPE_P2SH) {
        // undo spending activity
        erasebuffer.addressIndex.push_back(std::make_pair(
                    CAddressIndexKey(2, hash,
                        pindex->nHeight, txIndexInBlock, txid, txInputIndex, true),
                    prevout.nValue / SATOSHI * -1));

        // restore unspent index
        // WARNING: UndoCoinSpend NOW RETURNS A COIN INSTEAD OF A CTxInUndo (UNDO.nHeight NO LONGER EXITS, NOW IT'S GETHEIGHT)
        writebuffer.addressUnspentIndex.push_back(std::make_pair(
                    CAddressUnspentKey(2, hash,
                        input.prevout.GetTxId(), input.prevout.GetN()),
                    CAddressUnspentValue(prevout.nValue / SATOSHI,
                        prevout.scriptPubKey, undo.GetHeight())));
        return;
    }

    if (addressType == ADDRESSTYPE_P2PKH) {

        // undo spending activity
        erasebuffer.addressIndex.push_back(std::make_pair(
                    CAddressIndexKey(1, hash, pindex->nHeight, txIndexInBlock,
						txid, txInputIndex, true),
                    prevout.nValue / SATOSHI * -1));

        // restore unspent index
        // WARNING: UndoCoinSpend NOW RETURNS A COIN INSTEAD OF A CTxInUndo (UNDO.nHeight NO LONGER EXITS, NOW IT'S GETHEIGHT)
        writebuffer.addressUnspentIndex.push_back(std::make_pair(
                    CAddressUnspentKey(1, hash, input.prevout.GetTxId(),
                        input.prevout.GetN()),
                    CAddressUnspentValue(prevout.nValue / SATOSHI,
                        prevout.scriptPubKey, undo.GetHeight())));
        return;
    }
    assert(addressType == ADDRESSTYPE_UNKNOWN);
}

void AddressIndex::UndoCoinAdd(
	const size_t txInputIndex,
	const size_t txIndexInBlock,
	const CTransaction& tx,
	const CBlockIndex* pindex)
{
	const CTxOut &out = tx.vout[txInputIndex];
	const TxId& txid = tx.GetId();

	uint160 hash;
	int addressType;
	std::tie(hash, addressType) = GetHashAndAddressType(out);

	if (addressType == ADDRESSTYPE_P2SH) {
		// undo receiving activity
		erasebuffer.addressIndex.push_back(std::make_pair(
					CAddressIndexKey(2, hash, pindex->nHeight, txIndexInBlock,
						txid, txInputIndex, false),
					out.nValue / SATOSHI));

		// undo unspent index
		writebuffer.addressUnspentIndex.push_back(std::make_pair(
					CAddressUnspentKey(2, hash, txid, txInputIndex),
					CAddressUnspentValue()));
		return;
	}
	if (addressType == ADDRESSTYPE_P2PKH) {
		// undo receiving activity
		erasebuffer.addressIndex.push_back(std::make_pair(
					CAddressIndexKey(1, hash, pindex->nHeight, txIndexInBlock,
						txid, txInputIndex, false),
					out.nValue / SATOSHI));

		// undo unspent index
		writebuffer.addressUnspentIndex.push_back(std::make_pair(
					CAddressUnspentKey(1, hash, txid, txInputIndex),
					CAddressUnspentValue()));
		return;
	}
	assert(addressType == ADDRESSTYPE_UNKNOWN);

}

bool AddressIndex::WriteChanges() {
    std::string err;
    if (!writebuffer.addressIndex.empty() &&
            !m_db->WriteAddressIndex(writebuffer.addressIndex)) {
        err += "Failed to write address index.";
    }

    if (!writebuffer.addressUnspentIndex.empty() &&
            !m_db->UpdateAddressUnspentIndex(writebuffer.addressUnspentIndex)) {
        err += "Failed to write address unspent index.";
    }
    if (!erasebuffer.addressIndex.empty() &&
        !m_db->EraseAddressIndex(erasebuffer.addressIndex)) {
        err += "Failed to erase from address index.";
    }
    ClearBuffers();

    if (!err.empty()) {
        return AbortNode(err);
    }
    return true;
}


BaseIndex::DB& AddressIndex::GetDB() const {
    return *m_db;
}

bool AddressIndex::ReadAddressIndex(uint160 addressHash, int type,
        std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndexOut,
        int start, int end) {
    return m_db->ReadAddressIndex(addressHash, type, addressIndexOut, start, end);
}

bool AddressIndex::ReadAddressUnspentIndex(uint160 addressHash, int type,
                                           std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs)
{
    return m_db->ReadAddressUnspentIndex(addressHash, type, unspentOutputs);
}
