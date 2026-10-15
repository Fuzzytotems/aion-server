#include "aion/gameserver/network/aion/clientpackets/CM_ATREIAN_PASSPORT.h"

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/AtreianPassportService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_ATREIAN_PASSPORT::CM_ATREIAN_PASSPORT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_ATREIAN_PASSPORT.java:32-53. Java's `"..." + passports` is HashMap.toString ("{id=[timestamp, ...], ...}"); the C++ map's order
// is its own
void CM_ATREIAN_PASSPORT::readImpl() {
	int32_t count = readH();
	for (int32_t i = 0; i < count || count == -1; i++) {
		if (getRemainingBytes() < 8) {
			if (count != -1) {
				std::string data = "{";
				for (const auto& [passportId, timestamps] : passports) {
					if (data.size() > 1)
						data += ", ";
					data += std::to_string(passportId) + "=[";
					bool first = true;
					for (int32_t timestamp : timestamps) {
						data += (first ? "" : ", ") + std::to_string(timestamp);
						first = false;
					}
					data += "]";
				}
				data += "}";
				runtime::Ptr<model::gameobjects::player::Player> activePlayer = getConnection()->getActivePlayer();
				commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.clientpackets.CM_ATREIAN_PASSPORT")
					.warn("Received invalid passport count " + std::to_string(count) + " with only data for " + std::to_string(i) + " passports from " +
						(activePlayer ? activePlayer->toString() : std::string("null")) + "\nCurrent passport data: " + data);
			}
			break;
		}
		int32_t passportId = readD();
		int32_t timestamp = readD();
		passports[passportId].insert(timestamp); // Java: passports.compute(passportId, ...): a new HashSet for a new id, then add
	}
}

// Java CM_ATREIAN_PASSPORT.java:55-60
void CM_ATREIAN_PASSPORT::runImpl() {
	runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (player != nullptr)
		services::AtreianPassportService::getInstance().takeReward(*player, passports);
}

AION_CLIENT_PACKET(CM_ATREIAN_PASSPORT);

} // namespace aion::gameserver::network::aion::clientpackets
