#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when the player abandons a quest in his journal (C_GIVE_UP_QUEST): a timer quest's timer is stopped and
 * cleared on the client, then QuestService.abandonQuest drops the quest or puts a quest completed before back to COMPLETE.
 * <p>
 * C++ only: `CM_DELETE_QUESTTestAccess` (tests/cm_ak/DeleteQuestPacketTest.cpp) reads the field readImpl decoded, which Java keeps private
 * without a getter. Java names no author.
 */
class CM_DELETE_QUEST : public AionClientPacket {
	friend struct CM_DELETE_QUESTTestAccess;

private:
	int32_t questId{};

public:
	CM_DELETE_QUEST(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
