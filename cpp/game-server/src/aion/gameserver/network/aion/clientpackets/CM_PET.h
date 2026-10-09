#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/PetAction.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * Pet actions: adopt, surrender, summon, dismiss, feed, buff, auto loot/sell, rename, mood (C_PET_ACTION).
 *
 * @author M@xx, xTz
 */
class CM_PET : public AionClientPacket {
private:
	model::gameobjects::PetAction action = model::gameobjects::PetAction::UNKNOWN;
	int32_t templateId = 0;
	int32_t objectId = 0;
	std::string petName;
	int32_t decorationId = 0;
	int32_t eggObjId = 0;
	int32_t count = 0;
	int32_t subType = 0;
	int32_t emotionId = 0;
	int32_t actionType = 0;
	int32_t dopingItemId = 0;
	int32_t dopingAction = 0;
	int32_t dopingSlot1 = 0;
	int32_t dopingSlot2 = 0;
	int32_t activateSpecialFunction = 0;
	int32_t unk2 = 0, unk3 = 0, unk5 = 0, unk6 = 0; // Java @SuppressWarnings("unused")

public:
	CM_PET(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
