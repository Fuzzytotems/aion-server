#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/PetEmote.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A pet's movement and emotes, reported by its master's client (C_PET_EMOTE).
 *
 * @author ATracer
 */
class CM_PET_EMOTE : public AionClientPacket {
private:
	model::gameobjects::PetEmote emote = model::gameobjects::PetEmote::UNKNOWN;
	float x1 = 0, y1 = 0, z1 = 0, x2 = 0, y2 = 0, z2 = 0;
	int8_t h = 0;
	int32_t emoteId = 0, emotionId = 0;
	int32_t unk2 = 0;

	/** Java CM_PET_EMOTE.java:97-100: to every player who sees the pet, the master only `withMaster` */
	static void broadcastToSightedPlayers(model::gameobjects::Pet& pet, AionServerPacket& packet, bool withMaster);

public:
	CM_PET_EMOTE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
