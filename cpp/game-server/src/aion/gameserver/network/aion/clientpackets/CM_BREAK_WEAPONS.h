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
 * Breaks a fused weapon apart at an armsfusion officer (C_REMOVE_COMPOUND).
 *
 * @author zdead
 */
class CM_BREAK_WEAPONS : public AionClientPacket {
private:
	int32_t npcObjId = 0;
	int32_t weaponObjId = 0;

public:
	CM_BREAK_WEAPONS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
