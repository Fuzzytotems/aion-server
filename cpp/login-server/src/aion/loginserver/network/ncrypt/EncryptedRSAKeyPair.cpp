#include "aion/loginserver/network/ncrypt/EncryptedRSAKeyPair.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

namespace aion::loginserver::network::ncrypt {

using commons::utils::IllegalArgumentException;

EncryptedRSAKeyPair::EncryptedRSAKeyPair(EvpPkeyPtr rsaKeyPair) : rsaKeyPair(std::move(rsaKeyPair)) {
	if (!this->rsaKeyPair)
		throw IllegalArgumentException("RSA key pair must not be null");
	if (!EVP_PKEY_is_a(this->rsaKeyPair.get(), "RSA")) {
		const char* type = EVP_PKEY_get0_type_name(this->rsaKeyPair.get());
		throw IllegalArgumentException("Not an RSA key pair: " + std::string(type ? type : "unknown type"));
	}

	BIGNUM* n = nullptr;
	if (!EVP_PKEY_get_bn_param(this->rsaKeyPair.get(), OSSL_PKEY_PARAM_RSA_N, &n))
		throwOpenSslException("Could not read the RSA modulus");
	BignumPtr modulus(n);
	std::vector<uint8_t> magnitude(static_cast<size_t>(BN_num_bytes(modulus.get())));
	BN_bn2bin(modulus.get(), magnitude.data());
	encryptedModulus = encryptModulus(magnitude);
}

std::vector<uint8_t> EncryptedRSAKeyPair::encryptModulus(std::span<const uint8_t> modulus) {
	// BigInteger.toByteArray()
	auto first = std::ranges::find_if(modulus, [](uint8_t b) { return b != 0; });
	std::vector<uint8_t> encryptedModulus;
	if (first == modulus.end() || (*first & 0x80) != 0)
		encryptedModulus.push_back(0);
	encryptedModulus.insert(encryptedModulus.end(), first, modulus.end());

	if (encryptedModulus.size() == 0x81 && encryptedModulus[0] == 0x00)
		encryptedModulus.erase(encryptedModulus.begin());
	// Deviation: Java throws ArrayIndexOutOfBoundsException (a non-RSA key: ClassCastException in the constructor)
	if (encryptedModulus.size() < 0x80)
		throw IllegalArgumentException("RSA modulus is too short to be scrambled: " + std::to_string(encryptedModulus.size()) + " bytes");

	for (size_t i = 0; i < 4; i++)
		std::swap(encryptedModulus[i], encryptedModulus[0x4d + i]);
	for (size_t i = 0; i < 0x40; i++)
		encryptedModulus[i] ^= encryptedModulus[0x40 + i];
	for (size_t i = 0; i < 4; i++)
		encryptedModulus[0x0d + i] ^= encryptedModulus[0x34 + i];
	for (size_t i = 0; i < 0x40; i++)
		encryptedModulus[0x40 + i] ^= encryptedModulus[i];
	return encryptedModulus;
}

std::optional<std::vector<uint8_t>> EncryptedRSAKeyPair::decrypt(std::span<const uint8_t> encryptedData) const {
	if (encryptedData.size() % RSA_BLOCK_SIZE != 0)
		throw IllegalArgumentException("Bad arguments: RSA data size " + std::to_string(encryptedData.size()) + " is not a multiple of 128");

	std::vector<uint8_t> decrypted(encryptedData.size());
	if (encryptedData.empty())
		return decrypted;

	auto fail = [] {
		ERR_clear_error();
		return std::nullopt;
	};
	if (EVP_PKEY_get_size(rsaKeyPair.get()) != static_cast<int>(RSA_BLOCK_SIZE))
		return fail();
	EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_from_pkey(nullptr, rsaKeyPair.get(), nullptr));
	if (!ctx || EVP_PKEY_decrypt_init(ctx.get()) <= 0 || EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_NO_PADDING) <= 0)
		return fail();

	std::array<uint8_t, RSA_BLOCK_SIZE> block{};
	for (size_t offset = 0; offset < encryptedData.size(); offset += RSA_BLOCK_SIZE) {
		size_t blockLength = block.size();
		if (EVP_PKEY_decrypt(ctx.get(), block.data(), &blockLength, encryptedData.data() + offset, RSA_BLOCK_SIZE) <= 0 || blockLength > block.size())
			return fail();
		// left-pad like Java's RSACore.toByteArray(BigInteger, modulusLength) (OpenSSL already returns the full length for RSA_NO_PADDING)
		auto target = std::span(decrypted).subspan(offset, RSA_BLOCK_SIZE);
		std::ranges::fill(target.first(RSA_BLOCK_SIZE - blockLength), uint8_t{0});
		std::ranges::copy(std::span(block).first(blockLength), target.begin() + static_cast<ptrdiff_t>(RSA_BLOCK_SIZE - blockLength));
	}
	OPENSSL_cleanse(block.data(), block.size());
	return decrypted;
}

} // namespace aion::loginserver::network::ncrypt
