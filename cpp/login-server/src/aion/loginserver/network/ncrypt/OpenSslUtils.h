#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <openssl/types.h>

#include "aion/commons/utils/Exception.h"

/**
 * RAII helpers for the OpenSSL 3 objects used by the login server cryptography (no Java equivalent: the JCA manages these objects itself).
 */
namespace aion::loginserver::network::ncrypt {

/** Thrown when an OpenSSL call fails unexpectedly. The message contains the entries of the thread's OpenSSL error queue. */
class OpenSslException : public commons::utils::Exception {
public:
	using Exception::Exception;
};

struct EvpPkeyDeleter {
	void operator()(EVP_PKEY* key) const noexcept;
};
struct EvpPkeyCtxDeleter {
	void operator()(EVP_PKEY_CTX* ctx) const noexcept;
};
struct BignumDeleter {
	void operator()(BIGNUM* bn) const noexcept;
};
struct BnCtxDeleter {
	void operator()(BN_CTX* ctx) const noexcept;
};

/** Owning EVP_PKEY (key pair or public key) */
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;
/** Owning EVP_PKEY_CTX (one per operation; not shared between threads) */
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter>;
/** Owning BIGNUM (cleared before it is freed) */
using BignumPtr = std::unique_ptr<BIGNUM, BignumDeleter>;
/** Owning BN_CTX */
using BnCtxPtr = std::unique_ptr<BN_CTX, BnCtxDeleter>;

/** @return the entries of the calling thread's OpenSSL error queue separated by "; " (or "no OpenSSL error details"), clearing the queue */
std::string drainOpenSslErrors();

/** Throws OpenSslException("<message>: <drainOpenSslErrors()>") */
[[noreturn]] void throwOpenSslException(std::string_view message);

} // namespace aion::loginserver::network::ncrypt
