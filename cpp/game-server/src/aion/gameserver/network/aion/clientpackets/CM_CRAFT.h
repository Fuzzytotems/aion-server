#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player starts a craft at a crafting station, or morphs substances without one (C_COMBINE): an unsigned byte (129 for the morph, which
 * skips the station check), the station's template id, the recipe id, the station's object id, an unsigned short count, the craft type
 * (1 with an enhancement stone) and per material an item id and a count.
 * <p>
 * C++ only: `CM_CRAFTTestAccess` (tests/cm_ak/CraftPacketTest.cpp) reads the fields readImpl decoded, which Java keeps private without getters.
 *
 * @author Mr. Poke
 */
class CM_CRAFT : public AionClientPacket {
	friend struct CM_CRAFTTestAccess;

private:
	int32_t unk{};
	int32_t targetTemplateId{};
	int32_t recipeId{};
	int32_t targetObjId{};
	int32_t craftType{};
	std::unordered_map<int32_t, int64_t> materialsData; // Java: new HashMap<>()

public:
	CM_CRAFT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
