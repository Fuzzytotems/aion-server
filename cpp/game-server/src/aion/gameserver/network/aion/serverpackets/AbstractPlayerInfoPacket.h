#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * Base of the packets that write the character list entry of a player (SM_CHARACTER_LIST, SM_CREATE_CHARACTER).
 * <p>
 * S0c declaration header (hub-headers.md §12). C++ differences:
 * - Java's implicit constructor takes the opcode from the dynamic class; the C++ subclasses pass `opcodeOf<SM_X>` (AionServerPacket.h).
 * - writePlayerInfo reads the connection (getCharBanInfo: MultiClientingService.checkForFactionSwitchCooldownTime), so every subclass that
 *   calls it must be serialized per recipient (runtime-architecture.md §8.3).
 *
 * @author AEJTester, Nemesiss, Niato, Neon
 */
class AbstractPlayerInfoPacket : public AionServerPacket {
public:
	/** The maximum number of characters the client can display. The client expects a fixed size text buffer in various packets. */
	static constexpr int32_t CHARNAME_MAX_LENGTH = 25;

protected:
	/** Java: implicit constructor; C++: the subclass passes its opcode */
	explicit AbstractPlayerInfoPacket(int32_t opCode);

	void writePlayerInfo(model::account::PlayerAccountData& accPlData, AionConnection* con);

	void writeEquippedItems(const std::vector<runtime::Ptr<model::gameobjects::Item>>& items);

private:
	/** Returns the stored ban info or a new cooldown ban info (Java `new CharacterBanInfo(...)`), null if none applies */
	runtime::Ref<model::account::CharacterBanInfo> getCharBanInfo(model::account::PlayerAccountData& playerAccountData, AionConnection* con);
};

} // namespace aion::gameserver::network::aion::serverpackets
