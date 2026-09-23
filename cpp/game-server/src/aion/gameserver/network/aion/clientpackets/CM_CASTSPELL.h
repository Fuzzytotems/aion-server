#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet to start a skill cast (C_USE_SKILL), and with skill id 0 to cancel the cast in progress.
 * <p>
 * `receiveTime` is taken in the constructor, i.e. when the factory creates the packet on arrival (Java's field initializer runs in the constructor
 * the factory calls by reflection, AionClientPacketFactory.java:319), so the too-early check compares the arrival of this packet, not its
 * execution on the PacketProcessor, with the player's next allowed skill use.
 * <p>
 * C++ only: `CM_CASTSPELLTestAccess` (tests/cm_ak/CastSpellPacketTest.cpp) reads the fields readImpl decoded. Java's fields are private and
 * have no getters, and until SkillEngine.getSkillFor is ported (m5b2-plan.md S-01) nothing downstream of runImpl makes the level, the target
 * arm or the hit time observable.
 *
 * @author alexa026, rhys2002
 */
class CM_CASTSPELL : public AionClientPacket {
	friend struct CM_CASTSPELLTestAccess;

private:
	const int64_t receiveTime;
	int32_t spellid{};
	// 0 - obj id, 1 - point location, 2 - unk, 3 - object not in sight(skill 1606)? 4 - unk
	int32_t targetType{};
	float x{}, y{}, z{};

	/** Java: @SuppressWarnings("unused") - read from the body and never used */
	int32_t targetObjectId{};
	int32_t hitTime{};
	int32_t level{};
	/** Java: @SuppressWarnings("unused") - read from the body and never used */
	int32_t unk{};

public:
	CM_CASTSPELL(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
