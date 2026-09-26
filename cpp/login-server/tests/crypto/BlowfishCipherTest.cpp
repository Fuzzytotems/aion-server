#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// cross-check against OpenSSL's (deprecated) low-level Blowfish implementation
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/blowfish.h>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/loginserver/network/ncrypt/BlowfishCipher.h"

using namespace aion::commons::utils;
using aion::loginserver::network::ncrypt::BlowfishCipher;

namespace {

std::vector<uint8_t> hex(std::string_view text) {
	std::vector<uint8_t> bytes;
	for (size_t i = 0; i + 1 < text.size(); i += 2)
		bytes.push_back(static_cast<uint8_t>(std::stoi(std::string(text.substr(i, 2)), nullptr, 16)));
	return bytes;
}

/** Converts between textbook (big endian halves) and Aion (little endian halves) block byte order; the conversion is its own inverse. */
std::vector<uint8_t> swapHalves(std::vector<uint8_t> blocks) {
	for (size_t p = 0; p + 8 <= blocks.size(); p += 8) {
		std::reverse(blocks.begin() + static_cast<ptrdiff_t>(p), blocks.begin() + static_cast<ptrdiff_t>(p + 4));
		std::reverse(blocks.begin() + static_cast<ptrdiff_t>(p + 4), blocks.begin() + static_cast<ptrdiff_t>(p + 8));
	}
	return blocks;
}

std::vector<uint8_t> randomBytes(size_t size) {
	std::vector<uint8_t> bytes(size);
	Rnd::nextBytes(bytes);
	return bytes;
}

struct EcbVector {
	const char* key;
	const char* plain;
	const char* cipher;
};

// Eric Young's Blowfish ECB test vectors (https://www.schneier.com/code/vectors.txt), textbook big endian byte order
constexpr std::array<EcbVector, 34> ECB_VECTORS = {{
	{"0000000000000000", "0000000000000000", "4EF997456198DD78"},
	{"FFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF", "51866FD5B85ECB8A"},
	{"3000000000000000", "1000000000000001", "7D856F9A613063F2"},
	{"1111111111111111", "1111111111111111", "2466DD878B963C9D"},
	{"0123456789ABCDEF", "1111111111111111", "61F9C3802281B096"},
	{"1111111111111111", "0123456789ABCDEF", "7D0CC630AFDA1EC7"},
	{"0000000000000000", "0000000000000000", "4EF997456198DD78"},
	{"FEDCBA9876543210", "0123456789ABCDEF", "0ACEAB0FC6A0A28D"},
	{"7CA110454A1A6E57", "01A1D6D039776742", "59C68245EB05282B"},
	{"0131D9619DC1376E", "5CD54CA83DEF57DA", "B1B8CC0B250F09A0"},
	{"07A1133E4A0B2686", "0248D43806F67172", "1730E5778BEA1DA4"},
	{"3849674C2602319E", "51454B582DDF440A", "A25E7856CF2651EB"},
	{"04B915BA43FEB5B6", "42FD443059577FA2", "353882B109CE8F1A"},
	{"0113B970FD34F2CE", "059B5E0851CF143A", "48F4D0884C379918"},
	{"0170F175468FB5E6", "0756D8E0774761D2", "432193B78951FC98"},
	{"43297FAD38E373FE", "762514B829BF486A", "13F04154D69D1AE5"},
	{"07A7137045DA2A16", "3BDD119049372802", "2EEDDA93FFD39C79"},
	{"04689104C2FD3B2F", "26955F6835AF609A", "D887E0393C2DA6E3"},
	{"37D06BB516CB7546", "164D5E404F275232", "5F99D04F5B163969"},
	{"1F08260D1AC2465E", "6B056E18759F5CCA", "4A057A3B24D3977B"},
	{"584023641ABA6176", "004BD6EF09176062", "452031C1E4FADA8E"},
	{"025816164629B007", "480D39006EE762F2", "7555AE39F59B87BD"},
	{"49793EBC79B3258F", "437540C8698F3CFA", "53C55F9CB49FC019"},
	{"4FB05E1515AB73A7", "072D43A077075292", "7A8E7BFA937E89A3"},
	{"49E95D6D4CA229BF", "02FE55778117F12A", "CF9C5D7A4986ADB5"},
	{"018310DC409B26D6", "1D9D5C5018F728C2", "D1ABB290658BC778"},
	{"1C587F1C13924FEF", "305532286D6F295A", "55CB3774D13EF201"},
	{"0101010101010101", "0123456789ABCDEF", "FA34EC4847B268B2"},
	{"1F1F1F1F0E0E0E0E", "0123456789ABCDEF", "A790795108EA3CAE"},
	{"E0FEE0FEF1FEF1FE", "0123456789ABCDEF", "C39E072D9FAC631D"},
	{"0000000000000000", "FFFFFFFFFFFFFFFF", "014933E0CDAFF6E4"},
	{"FFFFFFFFFFFFFFFF", "0000000000000000", "F21E9A77B71C49BC"},
	{"0123456789ABCDEF", "0000000000000000", "245946885754369A"},
	{"FEDCBA9876543210", "FFFFFFFFFFFFFFFF", "6B5C5A9C5D9E0A5A"},
}};

// Eric Young's variable key length vectors: the first 1..24 bytes of this key encrypt FEDCBA9876543210
constexpr std::string_view SET_KEY_KEY = "F0E1D2C3B4A5968778695A4B3C2D1E0F0011223344556677";
constexpr std::string_view SET_KEY_PLAIN = "FEDCBA9876543210";
constexpr std::array<const char*, 24> SET_KEY_CIPHERS = {
	"F9AD597C49DB005E", "E91D21C1D961A6D6", "E9C2B70A1BC65CF3", "BE1E639408640F05", "B39E44481BDB1E6E", "9457AA83B1928C0D",
	"8BB77032F960629D", "E87A244E2CC85E82", "15750E7A4F4EC577", "122BA70B3AB64AE0", "3A833C9AFFC537F6", "9409DA87A90F6BF2",
	"884F80625060B8B4", "1F85031C19E11968", "79D9373A714CA34F", "93142887EE3BE15C", "03429E838CE2D14B", "A4299E27469FF67B",
	"AFD5AED1C1BC96A8", "10851C0E3858DA9F", "E6F51ED79B9DB21F", "64A6E14AFD36B46F", "80C7D7D45A5479AD", "05044B62FA52D080",
};

/** textbook ECB encryption/decryption with OpenSSL */
std::vector<uint8_t> openSslEcb(std::span<const uint8_t> key, std::vector<uint8_t> data, bool encrypt) {
	BF_KEY bfKey;
	BF_set_key(&bfKey, static_cast<int>(key.size()), key.data());
	for (size_t p = 0; p + 8 <= data.size(); p += 8)
		BF_ecb_encrypt(data.data() + p, data.data() + p, &bfKey, encrypt ? BF_ENCRYPT : BF_DECRYPT);
	return data;
}

} // namespace

TEST(BlowfishCipherTest, EricYoungEcbVectorsWithAionByteOrder) {
	for (const auto& vector : ECB_VECTORS) {
		SCOPED_TRACE(std::string(vector.key) + " " + vector.plain);
		BlowfishCipher cipher(hex(vector.key));

		std::vector<uint8_t> data = swapHalves(hex(vector.plain));
		cipher.cipher(data);
		EXPECT_EQ(data, swapHalves(hex(vector.cipher)));

		cipher.decipher(data);
		EXPECT_EQ(data, swapHalves(hex(vector.plain)));
	}
}

TEST(BlowfishCipherTest, EricYoungVectorsMatchOpenSsl) {
	// validates the vector table itself against an independent implementation
	for (const auto& vector : ECB_VECTORS)
		EXPECT_EQ(openSslEcb(hex(vector.key), hex(vector.plain), true), hex(vector.cipher)) << vector.key << " " << vector.plain;
	std::vector<uint8_t> key = hex(SET_KEY_KEY);
	for (size_t length = 1; length <= SET_KEY_CIPHERS.size(); length++)
		EXPECT_EQ(openSslEcb(std::span(key).first(length), hex(SET_KEY_PLAIN), true), hex(SET_KEY_CIPHERS[length - 1])) << length;
}

TEST(BlowfishCipherTest, EricYoungVariableKeyLengthVectors) {
	std::vector<uint8_t> key = hex(SET_KEY_KEY);
	for (size_t length = 1; length <= SET_KEY_CIPHERS.size(); length++) {
		SCOPED_TRACE(length);
		BlowfishCipher cipher(std::span(key).first(length));
		std::vector<uint8_t> data = swapHalves(hex(SET_KEY_PLAIN));
		cipher.cipher(data);
		EXPECT_EQ(data, swapHalves(hex(SET_KEY_CIPHERS[length - 1])));
		cipher.decipher(data);
		EXPECT_EQ(data, swapHalves(hex(SET_KEY_PLAIN)));
	}
}

TEST(BlowfishCipherTest, HandDerivedAionByteOrder) {
	// textbook: E(0, 0) = 4E F9 97 45 61 98 DD 78. Aion reads/writes each half little endian, so the halves appear byte-reversed.
	BlowfishCipher cipher(std::vector<uint8_t>(8, 0));
	std::array<uint8_t, 8> block{};
	cipher.cipher(block);
	EXPECT_EQ(block, (std::array<uint8_t, 8>{0x45, 0x97, 0xF9, 0x4E, 0x78, 0xDD, 0x98, 0x61}));

	// textbook plain 01 23 45 67 89 AB CD EF under key FEDCBA9876543210 -> 0A CE AB 0F C6 A0 A2 8D
	BlowfishCipher cipher2(hex("FEDCBA9876543210"));
	std::array<uint8_t, 8> block2 = {0x67, 0x45, 0x23, 0x01, 0xEF, 0xCD, 0xAB, 0x89};
	cipher2.cipher(block2);
	EXPECT_EQ(block2, (std::array<uint8_t, 8>{0x0F, 0xAB, 0xCE, 0x0A, 0x8D, 0xA2, 0xA0, 0xC6}));
}

TEST(BlowfishCipherTest, MatchesOpenSslForRandomKeysAndData) {
	for (int iteration = 0; iteration < 300; iteration++) {
		// key lengths beyond 72 bytes: only the first 72 bytes are used by both implementations
		std::vector<uint8_t> key = randomBytes(static_cast<size_t>(Rnd::get(1, 80)));
		std::vector<uint8_t> plain = randomBytes(8 * static_cast<size_t>(Rnd::get(1, 16)));
		SCOPED_TRACE(key.size());

		BlowfishCipher cipher(key);
		std::vector<uint8_t> data = plain;
		cipher.cipher(data);
		ASSERT_EQ(data, swapHalves(openSslEcb(key, swapHalves(plain), true)));
		cipher.decipher(data);
		ASSERT_EQ(data, plain);
	}
}

TEST(BlowfishCipherTest, RoundTripKeepsTrailingBytes) {
	BlowfishCipher cipher(randomBytes(16));
	for (size_t size = 0; size <= 40; size++) {
		std::vector<uint8_t> plain = randomBytes(size);
		std::vector<uint8_t> data = plain;
		cipher.cipher(data);
		size_t whole = size / 8 * 8;
		// trailing bytes are not touched
		EXPECT_TRUE(std::equal(data.begin() + static_cast<ptrdiff_t>(whole), data.end(), plain.begin() + static_cast<ptrdiff_t>(whole)));
		if (whole > 0)
			EXPECT_FALSE(std::equal(data.begin(), data.begin() + static_cast<ptrdiff_t>(whole), plain.begin()));
		cipher.decipher(data);
		EXPECT_EQ(data, plain);
	}
}

TEST(BlowfishCipherTest, EcbBlocksAreIndependent) {
	BlowfishCipher cipher(hex("0123456789ABCDEF"));
	std::vector<uint8_t> block = randomBytes(8);
	std::vector<uint8_t> twice = block;
	twice.insert(twice.end(), block.begin(), block.end());
	cipher.cipher(block);
	cipher.cipher(twice);
	EXPECT_TRUE(std::equal(block.begin(), block.end(), twice.begin()));
	EXPECT_TRUE(std::equal(block.begin(), block.end(), twice.begin() + 8));
}

TEST(BlowfishCipherTest, SubspanActsLikeJavaOffset) {
	BlowfishCipher cipher(randomBytes(16));
	std::vector<uint8_t> plain = randomBytes(27);
	std::vector<uint8_t> data = plain;
	cipher.cipher(std::span(data).subspan(3, 17)); // Java: cipher(data, 3, 17) - two blocks at 3..18
	EXPECT_TRUE(std::equal(data.begin(), data.begin() + 3, plain.begin()));
	EXPECT_TRUE(std::equal(data.begin() + 19, data.end(), plain.begin() + 19));
	std::vector<uint8_t> expected(plain.begin() + 3, plain.begin() + 19);
	cipher.cipher(expected);
	EXPECT_TRUE(std::equal(expected.begin(), expected.end(), data.begin() + 3));
}

TEST(BlowfishCipherTest, UpdateKeyReinitializesState) {
	std::vector<uint8_t> keyA = randomBytes(16);
	std::vector<uint8_t> keyB = randomBytes(16);
	std::vector<uint8_t> plain = randomBytes(32);

	BlowfishCipher fresh(keyB);
	std::vector<uint8_t> expected = plain;
	fresh.cipher(expected);

	BlowfishCipher updated(keyA);
	updated.updateKey(keyB);
	std::vector<uint8_t> data = plain;
	updated.cipher(data);
	EXPECT_EQ(data, expected);
}

TEST(BlowfishCipherTest, EmptyKeyThrows) {
	EXPECT_THROW(BlowfishCipher{std::span<const uint8_t>{}}, IllegalArgumentException);

	BlowfishCipher cipher(hex("0123456789ABCDEF"));
	std::vector<uint8_t> expected = randomBytes(8);
	std::vector<uint8_t> data = expected;
	cipher.cipher(expected);
	EXPECT_THROW(cipher.updateKey({}), IllegalArgumentException);
	cipher.cipher(data); // still the previous key
	EXPECT_EQ(data, expected);
}

TEST(BlowfishCipherTest, KeyIsCycled) {
	// a key and its repetition produce the same key schedule (as long as the repetition fits into the 72 bytes used)
	BlowfishCipher shortKey(hex("0123456789"));
	BlowfishCipher repeated(hex("01234567890123456789012345678901234567890123456789"));
	std::vector<uint8_t> a = randomBytes(24);
	std::vector<uint8_t> b = a;
	shortKey.cipher(a);
	repeated.cipher(b);
	EXPECT_EQ(a, b);
}
