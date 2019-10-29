// Copyright (c) 2019 The Bitcore ABC developers

#ifndef BITCOIN_INDEX_INDEXUTIL_H
#define BITCOIN_INDEX_INDEXUTIL_H

#include <utility>
#include <script/standard.h>

class uint160;
class CTxOut;

constexpr int ADDRESSTYPE_UNKNOWN = 0;
constexpr int ADDRESSTYPE_P2PKH = 1;
constexpr int ADDRESSTYPE_P2SH = 2;

std::pair<uint160, int> GetHashAndAddressType(const CTxOut& prevout);
std::pair<uint160, int> GetHashAndAddressType(const CTxDestination&);

#endif
