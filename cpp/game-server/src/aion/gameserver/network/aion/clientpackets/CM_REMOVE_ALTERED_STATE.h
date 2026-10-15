#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player right-clicks one of his own effect icons to end it early (C_TURN_OFF_ABNORMAL_STATUS).
 * <p>
 * C++ only: `CM_REMOVE_ALTERED_STATETestAccess` (tests/cm_lz/RemoveAlteredStatePacketTest.cpp) reads the skill id readImpl decoded, which Java
 * keeps in a private field without a getter; until EffectController.findBySkillId is ported (m5b2-plan.md K-02) runImpl cannot show it.
 *
 * @author dragoon112, Neon
 */
class CM_REMOVE_ALTERED_STATE : public AionClientPacket {
	friend struct CM_REMOVE_ALTERED_STATETestAccess;

private:
	int32_t skillId{};

public:
	CM_REMOVE_ALTERED_STATE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
