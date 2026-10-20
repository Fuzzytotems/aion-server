#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::worlds::levinshor {

struct VocolithAI_ItemUseObserver;

/**
 * The AI of a Vocolith in Levinshor ("LDF4_Advance_Vocolith"): while unused, a click starts a 1.5 s use bar (aborted by any disturbing action
 * of the player) that opens dialog 1011. Its first choice without a 185000216 fails (STR_MSG_LDF4_ADVANCE_FNAMED_FAIL, dialog NO_RIGHT); with
 * one, the first player to choose it uses it up, the Vocolith dies (its decay 7.5 s later) and 5 s later one of the four ancient monsters
 * (235217-235220) appears where it stood, told to the map. Any other end closes the dialog.
 * <p>
 * Java: data/handlers/ai/worlds/levinshor/VocolithAI.java, @AIName("LDF4_Advance_Vocolith") (the marker is in the .cpp).
 * <p>
 * C++: Java's anonymous ItemUseObserver is the callback struct VocolithAI_ItemUseObserver of the .cpp (fieldmap ai.worlds.levinshor.VocolithAI$1),
 * as ActionItemNpcAI's.
 *
 * @author Yeats, Neon
 */
class VocolithAI : public GeneralNpcAI {
	friend struct VocolithAI_ItemUseObserver;

private:
	runtime::AtomicBoolean used{AION_LOCK_CLASS(VocolithAI::used)}; // Java: = new AtomicBoolean()

public:
	explicit VocolithAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	void handleUsed(Player& player);

	void spawnRandomBoss();
};

} // namespace aion::gameserver::handlers::ai::worlds::levinshor
