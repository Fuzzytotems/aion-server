#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a hidden teleporter ("hidden_teleporter": eight npcs 804811-804825): its dialog's first choice starts the fly teleport of its
 * npc id.
 * <p>
 * Java: data/handlers/ai/HiddenTeleportNpcAI.java, @AIName("hidden_teleporter").
 */
class HiddenTeleportNpcAI : public NpcAI {
public:
	explicit HiddenTeleportNpcAI(Npc& owner) : NpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleDialogStart(Player& player) override;

private:
	void teleport(Player& player);
	int32_t getTeleportId();
};

} // namespace aion::gameserver::handlers::ai
