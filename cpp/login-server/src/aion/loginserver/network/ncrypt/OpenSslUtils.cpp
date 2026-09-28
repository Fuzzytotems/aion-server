#include "aion/loginserver/network/ncrypt/OpenSslUtils.h"

#include <array>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>

namespace aion::loginserver::network::ncrypt {

void EvpPkeyDeleter::operator()(EVP_PKEY* key) const noexcept {
	EVP_PKEY_free(key);
}

void EvpPkeyCtxDeleter::operator()(EVP_PKEY_CTX* ctx) const noexcept {
	EVP_PKEY_CTX_free(ctx);
}

void BignumDeleter::operator()(BIGNUM* bn) const noexcept {
	BN_clear_free(bn);
}

void BnCtxDeleter::operator()(BN_CTX* ctx) const noexcept {
	BN_CTX_free(ctx);
}

std::string drainOpenSslErrors() {
	std::string errors;
	std::array<char, 256> buffer{};
	while (unsigned long code = ERR_get_error()) {
		ERR_error_string_n(code, buffer.data(), buffer.size());
		if (!errors.empty())
			errors += "; ";
		errors += buffer.data();
	}
	return errors.empty() ? "no OpenSSL error details" : errors;
}

void throwOpenSslException(std::string_view message) {
	throw OpenSslException(std::string(message) + ": " + drainOpenSslErrors());
}

} // namespace aion::loginserver::network::ncrypt
