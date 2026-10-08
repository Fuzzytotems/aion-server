#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Renames the character or the legion by ticket, or uses a cosmetic item (C_ADDED_SERVICE_REQUEST).
 *
 * @author xTz, Neon
 */
class CM_APPEARANCE : public AionClientPacket {
private:
	int8_t type = 0;
	int32_t itemObjId = 0;
	std::string newName;

	void tryChangeCharacterName(model::gameobjects::player::Player& player, const std::string& newName, int32_t itemObjId);
	void tryChangeLegionName(model::gameobjects::player::Player& player, const std::string& newName, int32_t itemObjId);
	void tryUseCosmeticItem(model::gameobjects::player::Player& player, int32_t itemObjId);

public:
	CM_APPEARANCE(int32_t opcode, const StateSet& validStates);
	/** Java: public static onPlayerNameChanged (CM_APPEARANCE.java:90-98) */
	static void onPlayerNameChanged(model::gameobjects::player::Player& player, std::string_view oldName);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
