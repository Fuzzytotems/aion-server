#include "aion/gameserver/network/aion/AionClientPacket.h"

#include <functional>
#include <memory>
#include <set>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.AionClientPacket");

/**
 * C++ only (gameserver.network.trace.client_packets, play-session fixes 2026-09-28, docs/deviations/P4-15.md): logs the packet at INFO with the
 * name of its connection's player (the connection itself before a player entered the world) if the property names its class. run() is the one
 * place every client packet passes on its way to runImpl, so a play session can capture the client's packet sequence in the order it ran.
 */
static void traceIfConfigured(AionClientPacket& packet) {
	const std::shared_ptr<const std::set<std::string, std::less<>>> traced = configs::network::NetworkConfig::TRACE_CLIENT_PACKETS.get();
	if (traced->empty() || !traced->contains(packet.getPacketName()))
		return;
	const runtime::Ptr<model::gameobjects::player::Player> player = packet.getConnection()->getActivePlayer();
	log.info("Client packet trace: " + (player ? player->getName() : packet.getConnection()->toString()) + " sent " + packet.toString());
}

AionClientPacket::AionClientPacket(int32_t opcode, const StateSet& validStatesValue) : BaseClientPacket(opcode), validStates(validStatesValue) {
}

void AionClientPacket::run() {
	try {
		if (isValid()) { // run only if packet is still valid (connection state didn't change, for example due to logout)
			traceIfConfigured(*this); // C++ only (see above)
			runImpl();
		}
	} catch (...) {
		log.errorCurrentException("Error handling client packet from " + connectionToString() + ": " + toString());
	}
}

void AionClientPacket::sendPacket(AionServerPacket& msg) {
	getConnection()->sendPacket(msg);
}

std::string AionClientPacket::readS(int32_t characterCount) {
	std::string string = readS(); // read byte length = characters * 2 + 2
	const int32_t length = commons::utils::StringUtils::utf16Length(string);
	if (length < characterCount)
		readB((characterCount - length) * 2);
	return string;
}

bool AionClientPacket::isValid() {
	return validStates.contains(getConnection()->getState());
}

} // namespace aion::gameserver::network::aion
