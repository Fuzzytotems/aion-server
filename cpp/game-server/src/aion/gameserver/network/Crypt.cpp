#include "aion/gameserver/network/Crypt.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Rnd.h"

namespace aion::gameserver::network {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.Crypt"));
	return *logger;
}

} // namespace

int32_t Crypt::enableKey() {
	if (packetKey)
		throw commons::utils::IllegalStateException("Key is already initialized");

	// rnd key - this will be used to encrypt/decrypt packets
	return enableKey(commons::utils::Rnd::nextInt());
}

int32_t Crypt::enableKey(int32_t key) {
	if (packetKey)
		throw commons::utils::IllegalStateException("Key is already initialized");

	packetKey.emplace(key);

	if (log().isDebugEnabled())
		log().debug("new encrypt key: {}", packetKey->toString());

	// enciphered key that will be sent to aion client in SM_KEY packet
	return static_cast<int32_t>((static_cast<uint32_t>(key) ^ 0xCD92E4DFu) + 0x3FF2CCCFu);
}

bool Crypt::decrypt(std::span<uint8_t> data) {
	if (!packetKey)
		throw commons::utils::IllegalStateException("Cannot decrypt a client packet: the crypt key was not enabled (Java: NullPointerException)");
	return packetKey->decrypt(data);
}

void Crypt::encrypt(std::span<uint8_t> data) {
	if (!enabled) {
		// first server packet (SM_KEY) is not encrypted because it sends the enciphered crypt key
		enabled = true;
		return;
	}
	if (!packetKey)
		throw commons::utils::IllegalStateException("Cannot encrypt a server packet: the crypt key was not enabled (Java: NullPointerException)");
	packetKey->encrypt(data);
}

} // namespace aion::gameserver::network
