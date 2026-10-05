#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * A roll (mode 2) or a bid (mode 3) for an item the team distributes (C_GROUP_ITEM_DIST).
 *
 * @author Rhys2002
 */
class CM_GROUP_LOOT : public AionClientPacket {
private:
	[[maybe_unused]] int32_t groupId{}; // Java @SuppressWarnings("unused")
	int32_t index{};
	[[maybe_unused]] int32_t unk1{}; // Java @SuppressWarnings("unused")
	int32_t itemId{};
	[[maybe_unused]] int32_t unk2{}; // Java @SuppressWarnings("unused")
	[[maybe_unused]] int32_t unk3{}; // Java @SuppressWarnings("unused")
	int32_t npcObjId{};
	int32_t distributionMode{};
	int32_t roll{};
	int64_t bid{};
	[[maybe_unused]] int32_t unk4{}; // Java @SuppressWarnings("unused")
	[[maybe_unused]] int32_t unk5{}; // Java @SuppressWarnings("unused")

public:
	CM_GROUP_LOOT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
