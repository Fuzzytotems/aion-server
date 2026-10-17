#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

/**
 * The AI of the vault keeper of The Shugo Emperor's Vault ("IDSweep_Treasure_Room"): the first choice sends a player to one of the three
 * treasure rooms, the next room for each new player (a player keeps his room), and closes the dialog once all three are given.
 * <p>
 * Java: data/handlers/ai/instance/theShugoEmperorsVault/VaultKeeper.java, @AIName("IDSweep_Treasure_Room") (the marker is in the .cpp). Java's
 * synchronized checkEntryConditions is the monitor member. **Java bug, ported (proposed correction J-I6-1):** a player coming back to his room
 * also counts up `room` (room++ in every branch), so a returning player takes the next room away from the next new player; proposed: count up
 * only for a player not yet in playerAndRoom.
 *
 * @author Yeats
 */
class VaultKeeper : public GeneralNpcAI {
private:
	runtime::Field<int32_t> room{}; // Java: = 0
	/** Java: private Map<Integer, Integer> playerAndRoom = new HashMap<>() (guarded by the synchronized method) */
	runtime::HashMap<int32_t, int32_t> playerAndRoom{AION_LOCK_CLASS(VaultKeeper::playerAndRoom)};
	runtime::Monitor monitor{AION_LOCK_CLASS(VaultKeeper::monitor)}; // fieldmap: Java's synchronized checkEntryConditions

public:
	explicit VaultKeeper(Npc& owner) : GeneralNpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void checkEntryConditions(Player& player, int32_t dialogActionId);
};

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
