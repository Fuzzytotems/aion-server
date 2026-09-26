#include "aion/gameserver/utils/xml/CompressUtil.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::utils::xml {

namespace {

/** zlib's Adler-32 */
uint32_t adler32(std::span<const uint8_t> data) noexcept {
	constexpr uint32_t MOD_ADLER = 65521;
	uint32_t a = 1;
	uint32_t b = 0;
	for (uint8_t byte : data) {
		a = (a + byte) % MOD_ADLER;
		b = (b + a) % MOD_ADLER;
	}
	return (b << 16) | a;
}

// ---------------------------------------------------------------------------------------------------------------------------------- deflate

/*
 * The compressor is a port of zlib's deflate.c (deflate_slow, longest_match, fill_window) and trees.c (Huffman tree construction and block
 * emission) at compression level 6 with windowBits 15 and memLevel 8, the parameters of java.util.zip.Deflater(), so compress returns the bytes
 * of Java's (bundled) zlib. zlib is Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler, used under the zlib license.
 */

constexpr int32_t MIN_MATCH = 3;
constexpr int32_t MAX_MATCH = 258;
constexpr uint32_t W_SIZE = 1u << 15;
constexpr uint32_t W_MASK = W_SIZE - 1;
constexpr uint32_t WINDOW_SIZE = 2 * W_SIZE;
constexpr uint32_t HASH_SIZE = 1u << 15; // memLevel 8 + 7 bits
constexpr uint32_t HASH_MASK = HASH_SIZE - 1;
constexpr uint32_t HASH_SHIFT = (15 + MIN_MATCH - 1) / MIN_MATCH;
constexpr uint32_t MIN_LOOKAHEAD = MAX_MATCH + MIN_MATCH + 1;
constexpr uint32_t MAX_DIST = W_SIZE - MIN_LOOKAHEAD;
constexpr uint32_t WIN_INIT = MAX_MATCH;
constexpr uint32_t LIT_BUFSIZE = 1u << (8 + 6);
constexpr uint32_t SYM_END = (LIT_BUFSIZE - 1) * 3;
constexpr uint32_t TOO_FAR = 4096;
// configuration_table[6]
constexpr uint32_t GOOD_MATCH = 8;
constexpr uint32_t MAX_LAZY_MATCH = 16;
constexpr int32_t NICE_MATCH = 128;
constexpr uint32_t MAX_CHAIN = 128;

constexpr int32_t LENGTH_CODES = 29;
constexpr int32_t LITERALS = 256;
constexpr int32_t L_CODES = LITERALS + 1 + LENGTH_CODES;
constexpr int32_t D_CODES = 30;
constexpr int32_t BL_CODES = 19;
constexpr int32_t HEAP_SIZE = 2 * L_CODES + 1;
constexpr int32_t MAX_BITS = 15;
constexpr int32_t MAX_BL_BITS = 7;
constexpr int32_t END_BLOCK = 256;
constexpr int32_t REP_3_6 = 16;
constexpr int32_t REPZ_3_10 = 17;
constexpr int32_t REPZ_11_138 = 18;

constexpr std::array<int32_t, LENGTH_CODES> EXTRA_LBITS{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<int32_t, D_CODES> EXTRA_DBITS{0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
constexpr std::array<int32_t, BL_CODES> EXTRA_BLBITS{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7};
constexpr std::array<uint8_t, BL_CODES> BL_ORDER{16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/** trees.c ct_data: Freq and Code share a field in zlib, as do Dad and Len; the port keeps them apart where zlib never reads the other one */
struct CtData {
	uint16_t freq = 0;
	uint16_t code = 0;
	uint16_t len = 0; // also Dad while a tree is built
};

uint32_t biReverse(uint32_t code, int32_t len) noexcept {
	uint32_t res = 0;
	do {
		res |= code & 1;
		code >>= 1;
		res <<= 1;
	} while (--len > 0);
	return res >> 1;
}

void genCodes(CtData* tree, int32_t maxCode, const std::array<uint16_t, MAX_BITS + 1>& blCount) noexcept {
	std::array<uint16_t, MAX_BITS + 1> nextCode{};
	uint32_t code = 0;
	for (int32_t bits = 1; bits <= MAX_BITS; bits++) {
		code = (code + blCount[static_cast<size_t>(bits - 1)]) << 1;
		nextCode[static_cast<size_t>(bits)] = static_cast<uint16_t>(code);
	}
	for (int32_t n = 0; n <= maxCode; n++) {
		int32_t len = tree[n].len;
		if (len == 0)
			continue;
		tree[n].code = static_cast<uint16_t>(biReverse(nextCode[static_cast<size_t>(len)]++, len));
	}
}

/** trees.c tr_static_init: the static trees and the length and distance code tables */
struct StaticTables {
	std::array<CtData, L_CODES + 2> ltree{};
	std::array<CtData, D_CODES> dtree{};
	std::array<uint8_t, 512> distCode{};
	std::array<uint8_t, MAX_MATCH - MIN_MATCH + 1> lengthCode{};
	std::array<int32_t, LENGTH_CODES> baseLength{};
	std::array<int32_t, D_CODES> baseDist{};

	StaticTables() noexcept {
		int32_t length = 0;
		int32_t code = 0;
		for (code = 0; code < LENGTH_CODES - 1; code++) {
			baseLength[static_cast<size_t>(code)] = length;
			for (int32_t n = 0; n < (1 << EXTRA_LBITS[static_cast<size_t>(code)]); n++)
				lengthCode[static_cast<size_t>(length++)] = static_cast<uint8_t>(code);
		}
		lengthCode[static_cast<size_t>(length - 1)] = static_cast<uint8_t>(code); // match length 258 is code 28, not 27 + extra bits
		int32_t dist = 0;
		for (code = 0; code < 16; code++) {
			baseDist[static_cast<size_t>(code)] = dist;
			for (int32_t n = 0; n < (1 << EXTRA_DBITS[static_cast<size_t>(code)]); n++)
				distCode[static_cast<size_t>(dist++)] = static_cast<uint8_t>(code);
		}
		dist >>= 7;
		for (; code < D_CODES; code++) {
			baseDist[static_cast<size_t>(code)] = dist << 7;
			for (int32_t n = 0; n < (1 << (EXTRA_DBITS[static_cast<size_t>(code)] - 7)); n++)
				distCode[static_cast<size_t>(256 + dist++)] = static_cast<uint8_t>(code);
		}
		std::array<uint16_t, MAX_BITS + 1> blCount{};
		int32_t n = 0;
		while (n <= 143)
			ltree[static_cast<size_t>(n++)].len = 8, blCount[8]++;
		while (n <= 255)
			ltree[static_cast<size_t>(n++)].len = 9, blCount[9]++;
		while (n <= 279)
			ltree[static_cast<size_t>(n++)].len = 7, blCount[7]++;
		while (n <= 287)
			ltree[static_cast<size_t>(n++)].len = 8, blCount[8]++;
		genCodes(ltree.data(), L_CODES + 1, blCount);
		for (n = 0; n < D_CODES; n++) {
			dtree[static_cast<size_t>(n)].len = 5;
			dtree[static_cast<size_t>(n)].code = static_cast<uint16_t>(biReverse(static_cast<uint32_t>(n), 5));
		}
	}

	uint8_t dCode(uint32_t dist) const noexcept { return dist < 256 ? distCode[dist] : distCode[256 + (dist >> 7)]; }
};

const StaticTables& staticTables() {
	static const StaticTables tables;
	return tables;
}

/** trees.c tree_desc with its static_tree_desc */
struct TreeDesc {
	CtData* dynTree;
	int32_t maxCode = 0;
	const CtData* staticTree;
	const int32_t* extraBits;
	int32_t extraBase;
	int32_t elems;
	int32_t maxLength;
};

/** zlib's deflate state for a one-shot deflate(Z_FINISH) of the whole input */
class Deflater {
public:
	explicit Deflater(std::span<const uint8_t> input)
	    : input(input), tables(staticTables()), window(WINDOW_SIZE), prev(W_SIZE), head(HASH_SIZE), symBuf(LIT_BUFSIZE * 4) {
		lDesc = {dynLtree.data(), 0, tables.ltree.data(), EXTRA_LBITS.data(), LITERALS + 1, L_CODES, MAX_BITS};
		dDesc = {dynDtree.data(), 0, tables.dtree.data(), EXTRA_DBITS.data(), 0, D_CODES, MAX_BITS};
		blDesc = {blTree.data(), 0, nullptr, EXTRA_BLBITS.data(), 0, BL_CODES, MAX_BL_BITS};
		initBlock();
	}

	std::vector<uint8_t> run() {
		out.reserve(input.size() / 2 + 64);
		out.push_back(0x78); // CMF: deflate, 32K window
		out.push_back(0x9C); // FLG: default compression level (2 << 6), check bits
		deflateSlow();
		uint32_t checksum = adler32(input);
		for (int shift = 24; shift >= 0; shift -= 8)
			out.push_back(static_cast<uint8_t>(checksum >> shift));
		return std::move(out);
	}

private:
	// ---- deflate.c

	void updateHash(uint32_t c) noexcept { insH = ((insH << HASH_SHIFT) ^ c) & HASH_MASK; }

	/** INSERT_STRING: @return the previous head of the hash chain */
	uint32_t insertString(uint32_t str) noexcept {
		updateHash(window[str + (MIN_MATCH - 1)]);
		uint32_t matchHead = head[insH];
		prev[str & W_MASK] = static_cast<uint16_t>(matchHead);
		head[insH] = static_cast<uint16_t>(str);
		return matchHead;
	}

	void slideHash() noexcept {
		for (uint16_t& p : head)
			p = static_cast<uint16_t>(p >= W_SIZE ? p - W_SIZE : 0);
		for (uint16_t& p : prev)
			p = static_cast<uint16_t>(p >= W_SIZE ? p - W_SIZE : 0);
	}

	void fillWindow() noexcept {
		do {
			uint32_t more = WINDOW_SIZE - lookahead - strstart;
			if (strstart >= W_SIZE + MAX_DIST) {
				std::memcpy(window.data(), window.data() + W_SIZE, W_SIZE - more);
				matchStart -= W_SIZE;
				strstart -= W_SIZE;
				blockStart -= static_cast<int64_t>(W_SIZE);
				if (insert > strstart)
					insert = strstart;
				slideHash();
				more += W_SIZE;
			}
			if (inputPosition == input.size())
				break;
			uint32_t n = static_cast<uint32_t>(std::min<size_t>(more, input.size() - inputPosition));
			std::memcpy(window.data() + strstart + lookahead, input.data() + inputPosition, n);
			inputPosition += n;
			lookahead += n;
			if (lookahead + insert >= MIN_MATCH) {
				uint32_t str = strstart - insert;
				insH = window[str];
				updateHash(window[str + 1]);
				while (insert != 0) {
					updateHash(window[str + MIN_MATCH - 1]);
					prev[str & W_MASK] = head[insH];
					head[insH] = static_cast<uint16_t>(str);
					str++;
					insert--;
					if (lookahead + insert < MIN_MATCH)
						break;
				}
			}
		} while (lookahead < MIN_LOOKAHEAD && inputPosition != input.size());
		// zlib zeroes WIN_INIT bytes past the data so that longest_match never compares uninitialized bytes; the vector starts zeroed, so only
		// the high water mark matters (bytes above it keep older data after a slide, exactly like zlib's window)
		if (highWater < WINDOW_SIZE) {
			uint32_t curr = strstart + lookahead;
			if (highWater < curr) {
				uint32_t init = std::min(WINDOW_SIZE - curr, WIN_INIT);
				std::memset(window.data() + curr, 0, init);
				highWater = curr + init;
			} else if (highWater < curr + WIN_INIT) {
				uint32_t init = std::min(curr + WIN_INIT - highWater, WINDOW_SIZE - highWater);
				std::memset(window.data() + highWater, 0, init);
				highWater += init;
			}
		}
	}

	uint32_t longestMatch(uint32_t curMatch) noexcept {
		uint32_t chainLength = MAX_CHAIN;
		const uint8_t* scan = window.data() + strstart;
		int32_t bestLen = static_cast<int32_t>(prevLength);
		int32_t niceMatch = NICE_MATCH;
		uint32_t limit = strstart > MAX_DIST ? strstart - MAX_DIST : 0;
		const uint8_t* strend = window.data() + strstart + MAX_MATCH;
		uint8_t scanEnd1 = scan[bestLen - 1];
		uint8_t scanEnd = scan[bestLen];
		if (prevLength >= GOOD_MATCH)
			chainLength >>= 2;
		if (static_cast<uint32_t>(niceMatch) > lookahead)
			niceMatch = static_cast<int32_t>(lookahead);
		do {
			const uint8_t* match = window.data() + curMatch;
			if (match[bestLen] != scanEnd || match[bestLen - 1] != scanEnd1 || match[0] != scan[0] || match[1] != scan[1])
				continue;
			// scan[2] and match[2] are equal when the hashes are, zlib skips comparing them
			const uint8_t* s = scan + 2;
			const uint8_t* m = match + 2;
			while (s < strend && *++s == *++m) {
			}
			int32_t len = MAX_MATCH - static_cast<int32_t>(strend - s);
			if (len > bestLen) {
				matchStart = curMatch;
				bestLen = len;
				if (len >= niceMatch)
					break;
				scanEnd1 = scan[bestLen - 1];
				scanEnd = scan[bestLen];
			}
		} while ((curMatch = prev[curMatch & W_MASK]) > limit && --chainLength != 0);
		if (static_cast<uint32_t>(bestLen) <= lookahead)
			return static_cast<uint32_t>(bestLen);
		return lookahead;
	}

	void deflateSlow() {
		for (;;) {
			if (lookahead < MIN_LOOKAHEAD) {
				fillWindow();
				if (lookahead == 0)
					break; // flush is Z_FINISH
			}
			uint32_t hashHead = 0;
			if (lookahead >= MIN_MATCH)
				hashHead = insertString(strstart);
			prevLength = matchLength;
			prevMatch = matchStart;
			matchLength = MIN_MATCH - 1;
			if (hashHead != 0 && prevLength < MAX_LAZY_MATCH && strstart - hashHead <= MAX_DIST) {
				matchLength = longestMatch(hashHead);
				if (matchLength <= 5 && matchLength == MIN_MATCH && strstart - matchStart > TOO_FAR)
					matchLength = MIN_MATCH - 1; // a match of 3 this far away costs more than its literals
			}
			if (prevLength >= MIN_MATCH && matchLength <= prevLength) {
				uint32_t maxInsert = strstart + lookahead - MIN_MATCH;
				bool flush = tallyDist(strstart - 1 - prevMatch, prevLength - MIN_MATCH);
				lookahead -= prevLength - 1;
				prevLength -= 2;
				do {
					if (++strstart <= maxInsert)
						insertString(strstart);
				} while (--prevLength != 0);
				matchAvailable = false;
				matchLength = MIN_MATCH - 1;
				strstart++;
				if (flush)
					flushBlock(false);
			} else if (matchAvailable) {
				if (tallyLit(window[strstart - 1]))
					flushBlock(false);
				strstart++;
				lookahead--;
			} else {
				matchAvailable = true;
				strstart++;
				lookahead--;
			}
		}
		if (matchAvailable) {
			tallyLit(window[strstart - 1]);
			matchAvailable = false;
		}
		insert = strstart < MIN_MATCH - 1 ? strstart : MIN_MATCH - 1;
		flushBlock(true);
	}

	bool tallyLit(uint8_t c) noexcept {
		symBuf[symNext++] = 0;
		symBuf[symNext++] = 0;
		symBuf[symNext++] = c;
		dynLtree[c].freq++;
		return symNext == SYM_END;
	}

	bool tallyDist(uint32_t distance, uint32_t length) noexcept {
		symBuf[symNext++] = static_cast<uint8_t>(distance);
		symBuf[symNext++] = static_cast<uint8_t>(distance >> 8);
		symBuf[symNext++] = static_cast<uint8_t>(length);
		distance--;
		dynLtree[static_cast<size_t>(tables.lengthCode[length] + LITERALS + 1)].freq++;
		dynDtree[tables.dCode(distance)].freq++;
		return symNext == SYM_END;
	}

	/** FLUSH_BLOCK_ONLY */
	void flushBlock(bool last) {
		const uint8_t* buf = blockStart >= 0 ? window.data() + blockStart : nullptr;
		flushTreesBlock(buf, static_cast<uint32_t>(static_cast<int64_t>(strstart) - blockStart), last);
		blockStart = strstart;
	}

	// ---- trees.c

	void initBlock() noexcept {
		for (int32_t n = 0; n < L_CODES; n++)
			dynLtree[static_cast<size_t>(n)].freq = 0;
		for (int32_t n = 0; n < D_CODES; n++)
			dynDtree[static_cast<size_t>(n)].freq = 0;
		for (int32_t n = 0; n < BL_CODES; n++)
			blTree[static_cast<size_t>(n)].freq = 0;
		dynLtree[END_BLOCK].freq = 1;
		optLen = 0;
		staticLen = 0;
		symNext = 0;
	}

	bool smaller(const CtData* tree, int32_t n, int32_t m) const noexcept {
		return tree[n].freq < tree[m].freq || (tree[n].freq == tree[m].freq && depth[static_cast<size_t>(n)] <= depth[static_cast<size_t>(m)]);
	}

	void pqDownHeap(const CtData* tree, int32_t k) noexcept {
		int32_t v = heap[static_cast<size_t>(k)];
		int32_t j = k << 1;
		while (j <= heapLen) {
			if (j < heapLen && smaller(tree, heap[static_cast<size_t>(j + 1)], heap[static_cast<size_t>(j)]))
				j++;
			if (smaller(tree, v, heap[static_cast<size_t>(j)]))
				break;
			heap[static_cast<size_t>(k)] = heap[static_cast<size_t>(j)];
			k = j;
			j <<= 1;
		}
		heap[static_cast<size_t>(k)] = v;
	}

	void genBitlen(TreeDesc& desc) noexcept {
		CtData* tree = desc.dynTree;
		int32_t overflow = 0;
		blCount.fill(0);
		tree[heap[static_cast<size_t>(heapMax)]].len = 0; // root of the heap
		int32_t h = heapMax + 1;
		for (; h < HEAP_SIZE; h++) {
			int32_t n = heap[static_cast<size_t>(h)];
			int32_t bits = tree[tree[n].len].len + 1; // tree[n].len is still Dad here
			if (bits > desc.maxLength) {
				bits = desc.maxLength;
				overflow++;
			}
			tree[n].len = static_cast<uint16_t>(bits);
			if (n > desc.maxCode)
				continue; // not a leaf node
			blCount[static_cast<size_t>(bits)]++;
			int32_t xbits = n >= desc.extraBase ? desc.extraBits[n - desc.extraBase] : 0;
			uint32_t f = tree[n].freq;
			optLen += static_cast<uint64_t>(f) * static_cast<uint32_t>(bits + xbits);
			if (desc.staticTree != nullptr)
				staticLen += static_cast<uint64_t>(f) * static_cast<uint32_t>(desc.staticTree[n].len + xbits);
		}
		if (overflow == 0)
			return;
		// find the first bit length which could increase
		do {
			int32_t bits = desc.maxLength - 1;
			while (blCount[static_cast<size_t>(bits)] == 0)
				bits--;
			blCount[static_cast<size_t>(bits)]--;
			blCount[static_cast<size_t>(bits + 1)] += 2;
			blCount[static_cast<size_t>(desc.maxLength)]--;
			overflow -= 2;
		} while (overflow > 0);
		// recompute all bit lengths, scanning in increasing frequency
		for (int32_t bits = desc.maxLength; bits != 0; bits--) {
			int32_t n = blCount[static_cast<size_t>(bits)];
			while (n != 0) {
				int32_t m = heap[static_cast<size_t>(--h)];
				if (m > desc.maxCode)
					continue;
				if (tree[m].len != static_cast<uint32_t>(bits)) {
					optLen += (static_cast<uint64_t>(bits) - tree[m].len) * tree[m].freq;
					tree[m].len = static_cast<uint16_t>(bits);
				}
				n--;
			}
		}
	}

	void buildTree(TreeDesc& desc) noexcept {
		CtData* tree = desc.dynTree;
		int32_t maxCode = -1;
		heapLen = 0;
		heapMax = HEAP_SIZE;
		for (int32_t n = 0; n < desc.elems; n++) {
			if (tree[n].freq != 0) {
				heap[static_cast<size_t>(++heapLen)] = maxCode = n;
				depth[static_cast<size_t>(n)] = 0;
			} else {
				tree[n].len = 0;
			}
		}
		// the pkzip format requires that at least one distance code exists, and that at least one bit should be sent even if there is only one
		// possible code: at least two codes of non zero frequency
		while (heapLen < 2) {
			int32_t node = heap[static_cast<size_t>(++heapLen)] = maxCode < 2 ? ++maxCode : 0;
			tree[node].freq = 1;
			depth[static_cast<size_t>(node)] = 0;
			optLen--;
			if (desc.staticTree != nullptr)
				staticLen -= desc.staticTree[node].len;
		}
		desc.maxCode = maxCode;
		for (int32_t n = heapLen / 2; n >= 1; n--)
			pqDownHeap(tree, n);
		int32_t node = desc.elems;
		do {
			int32_t n = heap[1]; // pqremove
			heap[1] = heap[static_cast<size_t>(heapLen--)];
			pqDownHeap(tree, 1);
			int32_t m = heap[1];
			heap[static_cast<size_t>(--heapMax)] = n;
			heap[static_cast<size_t>(--heapMax)] = m;
			tree[node].freq = static_cast<uint16_t>(tree[n].freq + tree[m].freq);
			depth[static_cast<size_t>(node)] = static_cast<uint8_t>(
			  (depth[static_cast<size_t>(n)] >= depth[static_cast<size_t>(m)] ? depth[static_cast<size_t>(n)] : depth[static_cast<size_t>(m)]) + 1);
			tree[n].len = tree[m].len = static_cast<uint16_t>(node); // Dad
			heap[1] = node++;
			pqDownHeap(tree, 1);
		} while (heapLen >= 2);
		heap[static_cast<size_t>(--heapMax)] = heap[1];
		genBitlen(desc);
		genCodes(tree, maxCode, blCount);
	}

	void scanTree(CtData* tree, int32_t maxCode) noexcept {
		int32_t prevlen = -1;
		int32_t nextlen = tree[0].len;
		int32_t count = 0;
		int32_t maxCount = 7;
		int32_t minCount = 4;
		if (nextlen == 0) {
			maxCount = 138;
			minCount = 3;
		}
		tree[maxCode + 1].len = 0xFFFF; // guard
		for (int32_t n = 0; n <= maxCode; n++) {
			int32_t curlen = nextlen;
			nextlen = tree[n + 1].len;
			if (++count < maxCount && curlen == nextlen) {
				continue;
			} else if (count < minCount) {
				blTree[static_cast<size_t>(curlen)].freq = static_cast<uint16_t>(blTree[static_cast<size_t>(curlen)].freq + count);
			} else if (curlen != 0) {
				if (curlen != prevlen)
					blTree[static_cast<size_t>(curlen)].freq++;
				blTree[REP_3_6].freq++;
			} else if (count <= 10) {
				blTree[REPZ_3_10].freq++;
			} else {
				blTree[REPZ_11_138].freq++;
			}
			count = 0;
			prevlen = curlen;
			if (nextlen == 0) {
				maxCount = 138;
				minCount = 3;
			} else if (curlen == nextlen) {
				maxCount = 6;
				minCount = 3;
			} else {
				maxCount = 7;
				minCount = 4;
			}
		}
	}

	void sendTree(const CtData* tree, int32_t maxCode) {
		int32_t prevlen = -1;
		int32_t nextlen = tree[0].len;
		int32_t count = 0;
		int32_t maxCount = 7;
		int32_t minCount = 4;
		if (nextlen == 0) {
			maxCount = 138;
			minCount = 3;
		}
		for (int32_t n = 0; n <= maxCode; n++) {
			int32_t curlen = nextlen;
			nextlen = tree[n + 1].len;
			if (++count < maxCount && curlen == nextlen) {
				continue;
			} else if (count < minCount) {
				do {
					sendCode(curlen, blTree.data());
				} while (--count != 0);
			} else if (curlen != 0) {
				if (curlen != prevlen) {
					sendCode(curlen, blTree.data());
					count--;
				}
				sendCode(REP_3_6, blTree.data());
				sendBits(static_cast<uint32_t>(count - 3), 2);
			} else if (count <= 10) {
				sendCode(REPZ_3_10, blTree.data());
				sendBits(static_cast<uint32_t>(count - 3), 3);
			} else {
				sendCode(REPZ_11_138, blTree.data());
				sendBits(static_cast<uint32_t>(count - 11), 7);
			}
			count = 0;
			prevlen = curlen;
			if (nextlen == 0) {
				maxCount = 138;
				minCount = 3;
			} else if (curlen == nextlen) {
				maxCount = 6;
				minCount = 3;
			} else {
				maxCount = 7;
				minCount = 4;
			}
		}
	}

	int32_t buildBlTree() noexcept {
		scanTree(dynLtree.data(), lDesc.maxCode);
		scanTree(dynDtree.data(), dDesc.maxCode);
		buildTree(blDesc);
		int32_t maxBlindex = BL_CODES - 1;
		for (; maxBlindex >= 3; maxBlindex--) {
			if (blTree[BL_ORDER[static_cast<size_t>(maxBlindex)]].len != 0)
				break;
		}
		optLen += 3 * (static_cast<uint64_t>(maxBlindex) + 1) + 5 + 5 + 4;
		return maxBlindex;
	}

	void sendAllTrees(int32_t lcodes, int32_t dcodes, int32_t blcodes) {
		sendBits(static_cast<uint32_t>(lcodes - 257), 5);
		sendBits(static_cast<uint32_t>(dcodes - 1), 5);
		sendBits(static_cast<uint32_t>(blcodes - 4), 4);
		for (int32_t rank = 0; rank < blcodes; rank++)
			sendBits(blTree[BL_ORDER[static_cast<size_t>(rank)]].len, 3);
		sendTree(dynLtree.data(), lcodes - 1);
		sendTree(dynDtree.data(), dcodes - 1);
	}

	void compressBlock(const CtData* ltree, const CtData* dtree) {
		for (uint32_t sx = 0; sx < symNext;) {
			uint32_t dist = symBuf[sx++];
			dist += static_cast<uint32_t>(symBuf[sx++]) << 8;
			int32_t lc = symBuf[sx++];
			if (dist == 0) {
				sendCode(lc, ltree); // a literal byte
			} else {
				int32_t code = tables.lengthCode[static_cast<size_t>(lc)];
				sendCode(code + LITERALS + 1, ltree);
				int32_t extra = EXTRA_LBITS[static_cast<size_t>(code)];
				if (extra != 0)
					sendBits(static_cast<uint32_t>(lc - tables.baseLength[static_cast<size_t>(code)]), extra);
				dist--;
				code = tables.dCode(dist);
				sendCode(code, dtree);
				extra = EXTRA_DBITS[static_cast<size_t>(code)];
				if (extra != 0)
					sendBits(dist - static_cast<uint32_t>(tables.baseDist[static_cast<size_t>(code)]), extra);
			}
		}
		sendCode(END_BLOCK, ltree);
	}

	/** _tr_flush_block */
	void flushTreesBlock(const uint8_t* buf, uint32_t storedLen, bool last) {
		buildTree(lDesc);
		buildTree(dDesc);
		int32_t maxBlindex = buildBlTree();
		uint64_t optLenb = (optLen + 3 + 7) >> 3;
		uint64_t staticLenb = (staticLen + 3 + 7) >> 3;
		if (staticLenb <= optLenb)
			optLenb = staticLenb;
		if (static_cast<uint64_t>(storedLen) + 4 <= optLenb && buf != nullptr) {
			sendBits(last ? 1u : 0u, 3); // STORED_BLOCK
			biWindup();
			putByte(static_cast<uint8_t>(storedLen));
			putByte(static_cast<uint8_t>(storedLen >> 8));
			putByte(static_cast<uint8_t>(~storedLen));
			putByte(static_cast<uint8_t>(~storedLen >> 8));
			out.insert(out.end(), buf, buf + storedLen);
		} else if (staticLenb == optLenb) {
			sendBits((1u << 1) + (last ? 1u : 0u), 3); // STATIC_TREES
			compressBlock(tables.ltree.data(), tables.dtree.data());
		} else {
			sendBits((2u << 1) + (last ? 1u : 0u), 3); // DYN_TREES
			sendAllTrees(lDesc.maxCode + 1, dDesc.maxCode + 1, maxBlindex + 1);
			compressBlock(dynLtree.data(), dynDtree.data());
		}
		initBlock();
		if (last)
			biWindup();
	}

	void sendCode(int32_t c, const CtData* tree) { sendBits(tree[c].code, tree[c].len); }

	void sendBits(uint32_t value, int32_t length) {
		bitBuffer |= static_cast<uint64_t>(value & ((1u << length) - 1)) << bitCount;
		bitCount += length;
		while (bitCount >= 8) {
			out.push_back(static_cast<uint8_t>(bitBuffer));
			bitBuffer >>= 8;
			bitCount -= 8;
		}
	}

	void biWindup() {
		if (bitCount > 0)
			out.push_back(static_cast<uint8_t>(bitBuffer));
		bitBuffer = 0;
		bitCount = 0;
	}

	void putByte(uint8_t value) { out.push_back(value); }

	std::span<const uint8_t> input;
	size_t inputPosition = 0;
	const StaticTables& tables;
	std::vector<uint8_t> out;

	std::vector<uint8_t> window;
	std::vector<uint16_t> prev;
	std::vector<uint16_t> head;
	uint32_t insH = 0;
	int64_t blockStart = 0;
	uint32_t matchLength = MIN_MATCH - 1;
	uint32_t prevMatch = 0;
	bool matchAvailable = false;
	uint32_t strstart = 0;
	uint32_t matchStart = 0;
	uint32_t lookahead = 0;
	uint32_t prevLength = MIN_MATCH - 1;
	uint32_t insert = 0;
	uint32_t highWater = 0;

	std::array<CtData, HEAP_SIZE> dynLtree{};
	std::array<CtData, 2 * D_CODES + 1> dynDtree{};
	std::array<CtData, 2 * BL_CODES + 1> blTree{};
	TreeDesc lDesc{};
	TreeDesc dDesc{};
	TreeDesc blDesc{};
	std::array<uint16_t, MAX_BITS + 1> blCount{};
	std::array<int32_t, 2 * L_CODES + 1> heap{};
	int32_t heapLen = 0;
	int32_t heapMax = 0;
	std::array<uint8_t, 2 * L_CODES + 1> depth{};
	std::vector<uint8_t> symBuf;
	uint32_t symNext = 0;
	uint64_t optLen = 0;
	uint64_t staticLen = 0;
	uint64_t bitBuffer = 0;
	int32_t bitCount = 0;
};

// ---------------------------------------------------------------------------------------------------------------------------------- inflate

struct TruncatedInput {};

[[noreturn]] void dataFormatError(const char* message) {
	throw runtime::IllegalArgumentException(message); // Java: java.util.zip.DataFormatException with zlib's message
}

/** RFC 1951 inflater (the structure of zlib's puff.c) with the checks and messages of zlib's inflate.c and inflate_table */
class Inflater {
public:
	explicit Inflater(std::span<const uint8_t> input) : input(input) {}

	/** @return the number of input bytes consumed by the zlib stream */
	size_t inflateZlib(std::vector<uint8_t>& output) {
		uint32_t cmf = nextByte();
		uint32_t flg = nextByte();
		if (((cmf << 8) | flg) % 31 != 0)
			dataFormatError("incorrect header check");
		if ((cmf & 0x0F) != 8)
			dataFormatError("unknown compression method");
		if ((cmf >> 4) + 8 > 15)
			dataFormatError("invalid window size");
		if (flg & 0x20)
			throw TruncatedInput{}; // Java: Inflater.needsDictionary() makes inflate return 0, so decompress throws "Bad zip data"
		out = &output;
		bool last;
		do {
			last = bits(1) == 1;
			switch (bits(2)) {
				case 0:
					stored();
					break;
				case 1:
					fixed();
					break;
				case 2:
					dynamic();
					break;
				default:
					dataFormatError("invalid block type");
			}
		} while (!last);
		bitBuffer = 0;
		bitCount = 0;
		uint32_t expected = 0;
		for (int i = 0; i < 4; ++i)
			expected = (expected << 8) | nextByte();
		if (expected != adler32(output))
			dataFormatError("incorrect data check");
		return position;
	}

private:
	struct Huffman {
		std::array<uint16_t, 16> count{};
		std::vector<uint16_t> symbol;
	};

	/** the zlib code table a Huffman code belongs to, for inflate_table's completeness rules and the decoding error messages */
	enum class TableType { CODES, LENS, DISTS };

	uint8_t nextByte() {
		if (position >= input.size())
			throw TruncatedInput{};
		return input[position++];
	}

	uint32_t bits(int need) {
		uint64_t value = bitBuffer;
		while (bitCount < need) {
			value |= static_cast<uint64_t>(nextByte()) << bitCount;
			bitCount += 8;
		}
		bitBuffer = value >> need;
		bitCount -= need;
		return static_cast<uint32_t>(value & ((1ull << need) - 1));
	}

	void stored() {
		bitBuffer = 0;
		bitCount = 0;
		uint32_t len = nextByte();
		len |= static_cast<uint32_t>(nextByte()) << 8;
		uint32_t nlen = nextByte();
		nlen |= static_cast<uint32_t>(nextByte()) << 8;
		if (len != (~nlen & 0xFFFF))
			dataFormatError("invalid stored block lengths");
		while (len-- > 0)
			out->push_back(nextByte());
	}

	int32_t decode(const Huffman& h, TableType type) {
		if (h.count[0] == h.symbol.size()) {
			// inflate_table for a code without any symbols: every entry is invalid and one bit long; the code length code reads it as length 0
			if (type == TableType::CODES) {
				bits(1);
				return 0;
			}
			bits(1);
			dataFormatError(type == TableType::LENS ? "invalid literal/length code" : "invalid distance code");
		}
		size_t symbols = h.symbol.size() - h.count[0];
		int32_t code = 0;
		int32_t first = 0;
		int32_t index = 0;
		for (int len = 1; len <= 15; ++len) {
			code |= static_cast<int32_t>(bits(1));
			int32_t count = h.count[static_cast<size_t>(len)];
			if (code - count < first)
				return h.symbol[static_cast<size_t>(index + (code - first))];
			index += count;
			if (static_cast<size_t>(index) == symbols)
				break; // no longer codes: zlib's table marks the unused pattern invalid without reading more bits
			first += count;
			first <<= 1;
			code <<= 1;
		}
		// only an incomplete code (a single code of length 1) has unused bit patterns
		dataFormatError(type == TableType::DISTS ? "invalid distance code" : "invalid literal/length code");
	}

	/**
	 * @return true if zlib's inflate_table accepts the code lengths: never over-subscribed; incomplete only for literal/length and distance codes
	 *         whose longest code has one bit (a single code); a code without any symbols is accepted
	 */
	static bool construct(Huffman& h, const uint16_t* length, int32_t n, TableType type) {
		h.count.fill(0);
		h.symbol.assign(static_cast<size_t>(n), 0);
		for (int32_t symbolIndex = 0; symbolIndex < n; ++symbolIndex)
			h.count[length[symbolIndex]]++;
		if (h.count[0] == n)
			return true;
		int32_t left = 1;
		int32_t max = 0;
		for (int len = 1; len <= 15; ++len) {
			left <<= 1;
			left -= h.count[static_cast<size_t>(len)];
			if (left < 0)
				return false; // over-subscribed
			if (h.count[static_cast<size_t>(len)] != 0)
				max = len;
		}
		if (left > 0 && (type == TableType::CODES || max != 1))
			return false; // incomplete set
		std::array<uint16_t, 16> offs{};
		for (int len = 1; len < 15; ++len)
			offs[static_cast<size_t>(len + 1)] = static_cast<uint16_t>(offs[static_cast<size_t>(len)] + h.count[static_cast<size_t>(len)]);
		for (int32_t symbolIndex = 0; symbolIndex < n; ++symbolIndex) {
			if (length[symbolIndex] != 0)
				h.symbol[offs[length[symbolIndex]]++] = static_cast<uint16_t>(symbolIndex);
		}
		return true;
	}

	void codes(const Huffman& lencode, const Huffman& distcode) {
		static constexpr std::array<uint16_t, 29> LENGTH_BASE{3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
		                                                      31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
		static constexpr std::array<uint16_t, 29> LENGTH_EXTRA{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
		static constexpr std::array<uint16_t, 30> DIST_BASE{1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
		                                                    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
		static constexpr std::array<uint16_t, 30> DIST_EXTRA{0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
		                                                     6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
		for (;;) {
			int32_t symbol = decode(lencode, TableType::LENS);
			if (symbol < 256) {
				out->push_back(static_cast<uint8_t>(symbol));
			} else if (symbol == 256) {
				return;
			} else {
				symbol -= 257;
				if (symbol >= 29)
					dataFormatError("invalid literal/length code");
				size_t len = LENGTH_BASE[static_cast<size_t>(symbol)] + bits(LENGTH_EXTRA[static_cast<size_t>(symbol)]);
				int32_t distSymbol = decode(distcode, TableType::DISTS);
				if (distSymbol >= 30)
					dataFormatError("invalid distance code");
				size_t dist = DIST_BASE[static_cast<size_t>(distSymbol)] + bits(DIST_EXTRA[static_cast<size_t>(distSymbol)]);
				if (dist > out->size())
					dataFormatError("invalid distance too far back");
				size_t from = out->size() - dist;
				for (size_t i = 0; i < len; ++i)
					out->push_back((*out)[from + i]);
			}
		}
	}

	void fixed() {
		std::array<uint16_t, 320> lengths{};
		size_t symbol = 0;
		for (; symbol < 144; ++symbol)
			lengths[symbol] = 8;
		for (; symbol < 256; ++symbol)
			lengths[symbol] = 9;
		for (; symbol < 280; ++symbol)
			lengths[symbol] = 7;
		for (; symbol < 288; ++symbol)
			lengths[symbol] = 8;
		Huffman lencode;
		construct(lencode, lengths.data(), 288, TableType::LENS);
		for (symbol = 0; symbol < 30; ++symbol)
			lengths[symbol] = 5;
		Huffman distcode;
		construct(distcode, lengths.data(), 30, TableType::DISTS);
		codes(lencode, distcode);
	}

	void dynamic() {
		static constexpr std::array<uint8_t, 19> ORDER{16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
		std::array<uint16_t, 320> lengths{};
		int32_t nlen = static_cast<int32_t>(bits(5)) + 257;
		int32_t ndist = static_cast<int32_t>(bits(5)) + 1;
		int32_t ncode = static_cast<int32_t>(bits(4)) + 4;
		if (nlen > 286 || ndist > 30)
			dataFormatError("too many length or distance symbols");
		int32_t index = 0;
		for (; index < ncode; ++index)
			lengths[ORDER[static_cast<size_t>(index)]] = static_cast<uint16_t>(bits(3));
		for (; index < 19; ++index)
			lengths[ORDER[static_cast<size_t>(index)]] = 0;
		Huffman lencode;
		if (!construct(lencode, lengths.data(), 19, TableType::CODES))
			dataFormatError("invalid code lengths set");
		index = 0;
		while (index < nlen + ndist) {
			int32_t symbol = decode(lencode, TableType::CODES);
			if (symbol < 16) {
				lengths[static_cast<size_t>(index++)] = static_cast<uint16_t>(symbol);
			} else {
				uint16_t len = 0;
				uint32_t repeat;
				if (symbol == 16) {
					if (index == 0)
						dataFormatError("invalid bit length repeat");
					len = lengths[static_cast<size_t>(index - 1)];
					repeat = 3 + bits(2);
				} else if (symbol == 17) {
					repeat = 3 + bits(3);
				} else {
					repeat = 11 + bits(7);
				}
				if (index + static_cast<int32_t>(repeat) > nlen + ndist)
					dataFormatError("invalid bit length repeat");
				while (repeat-- > 0)
					lengths[static_cast<size_t>(index++)] = len;
			}
		}
		if (lengths[256] == 0)
			dataFormatError("invalid code -- missing end-of-block");
		if (!construct(lencode, lengths.data(), nlen, TableType::LENS))
			dataFormatError("invalid literal/lengths set");
		Huffman distcode;
		if (!construct(distcode, lengths.data() + nlen, ndist, TableType::DISTS))
			dataFormatError("invalid distances set");
		codes(lencode, distcode);
	}

	std::span<const uint8_t> input;
	size_t position = 0;
	uint64_t bitBuffer = 0;
	int32_t bitCount = 0;
	std::vector<uint8_t>* out = nullptr;
};

} // namespace

std::vector<uint8_t> CompressUtil::decompress(std::span<const uint8_t> bytes) {
	std::vector<uint8_t> output;
	output.reserve(bytes.size());
	try {
		Inflater(bytes).inflateZlib(output);
	} catch (const TruncatedInput&) {
		throw runtime::IllegalStateException("Bad zip data, size: " + std::to_string(bytes.size()));
	}
	return output;
}

std::vector<uint8_t> CompressUtil::compress(std::span<const uint8_t> bytes) {
	return Deflater(bytes).run();
}

} // namespace aion::gameserver::utils::xml
