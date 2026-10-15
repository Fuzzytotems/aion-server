#pragma once

#include <cstdint>

#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The loot rules of the sender's group, alliance or league (C_LOOT_RULE).
 *
 * @author Lyahim, Simple, xTz
 */
class CM_DISTRIBUTION_SETTINGS : public AionClientPacket {
private:
	[[maybe_unused]] int32_t isLeague{}; // Java @SuppressWarnings("unused")
	int32_t lootRule{};
	int32_t misc{};
	model::team::common::legacy::LootRuleType lootRules{};
	int32_t commonItemAbove{};
	int32_t superiorItemAbove{};
	int32_t heroicItemAbove{};
	int32_t fabledItemAbove{};
	int32_t ethernalItemAbove{};
	int32_t mythicItemAbove{};
	[[maybe_unused]] int32_t unk{}; // Java @SuppressWarnings("unused")

public:
	CM_DISTRIBUTION_SETTINGS(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
