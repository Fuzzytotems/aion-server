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
 * Fuses two weapons at an armsfusion officer (C_COMPOUND_2H_WEAPON).
 *
 * @author zdead, Wakizashi, Neon
 */
class CM_FUSION_WEAPONS : public AionClientPacket {
private:
	int32_t npcObjId = 0;
	int32_t mainWeaponObjId = 0;
	int32_t fuseWeaponObjId = 0;

public:
	CM_FUSION_WEAPONS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
