#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The game client sends this packet when selecting a target via click, hotkey or the chat commands /select name and /selecttargetoftarget.
 *
 * @author SoulKeeper, Sweetkr, KID
 */
class CM_TARGET_SELECT : public AionClientPacket {
private:
	/** Target object id that client wants to select or 0 if wants to unselect */
	int32_t targetObjectId{};
	bool selectTargetOfTarget{};

public:
	CM_TARGET_SELECT(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
