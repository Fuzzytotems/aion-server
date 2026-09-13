#include "aion/loginserver/network/ncrypt/KeyGen.h"

#include <mutex>
#include <vector>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"

namespace aion::loginserver::network::ncrypt::KeyGen {

namespace {

using KeyPairs = std::vector<std::shared_ptr<const EncryptedRSAKeyPair>>;

struct State {
	std::mutex mutex;
	KeyPairs encryptedRSAKeyPairs;
	bool initialized = false;
};

/**
 * Leaked on purpose: OpenSSL registers its cleanup with atexit when it is first used, which may be after this object would have been
 * constructed, so a destructor freeing the keys could run after OpenSSL was shut down.
 */
State& state() {
	static auto* s = new State();
	return *s;
}

EvpPkeyPtr generateRSAKeyPair() {
	EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr));
	if (!ctx || EVP_PKEY_keygen_init(ctx.get()) <= 0)
		throwOpenSslException("Could not initialize the RSA key pair generator");
	BignumPtr publicExponent(BN_new());
	if (!publicExponent || !BN_set_word(publicExponent.get(), RSA_F4))
		throwOpenSslException("Could not create the RSA public exponent");
	if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 1024) <= 0 || EVP_PKEY_CTX_set1_rsa_keygen_pubexp(ctx.get(), publicExponent.get()) <= 0)
		throwOpenSslException("Could not set the RSA key generation parameters");
	EVP_PKEY* key = nullptr;
	if (EVP_PKEY_generate(ctx.get(), &key) <= 0)
		throwOpenSslException("Could not generate an RSA key pair");
	return EvpPkeyPtr(key);
}

} // namespace

void init() {
	KeyPairs pairs;
	pairs.reserve(RSA_KEY_PAIR_COUNT);
	for (size_t i = 0; i < RSA_KEY_PAIR_COUNT; i++)
		pairs.push_back(std::make_shared<const EncryptedRSAKeyPair>(generateRSAKeyPair()));

	State& s = state();
	std::scoped_lock lock(s.mutex);
	s.encryptedRSAKeyPairs.swap(pairs);
	s.initialized = true;
}

BlowfishKey generateBlowfishKey() {
	// Deviation (before init): Java throws a NullPointerException here and returns null from getEncryptedRSAKeyPair
	{
		State& s = state();
		std::scoped_lock lock(s.mutex);
		if (!s.initialized)
			throw commons::utils::IllegalStateException("KeyGen is not initialized");
	}
	BlowfishKey key{};
	if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1)
		throwOpenSslException("Could not generate a Blowfish key");
	return key;
}

std::shared_ptr<const EncryptedRSAKeyPair> getEncryptedRSAKeyPair() {
	State& s = state();
	std::scoped_lock lock(s.mutex);
	if (!s.initialized)
		throw commons::utils::IllegalStateException("KeyGen is not initialized");
	return *commons::utils::Rnd::get(s.encryptedRSAKeyPairs);
}

} // namespace aion::loginserver::network::ncrypt::KeyGen
