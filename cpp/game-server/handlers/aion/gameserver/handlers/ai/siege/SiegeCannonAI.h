#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * A sky cannon of a fortress ("siege_cannon"): its dialog's SETPRO1 puts the player in its place, morphs him with the cannon's skill (by npc id:
 * 21517/21518 for Krotan, Kysis, Miren and the Divine fortress, 21385/21386 for Wealhtheow, level 65) and deletes the cannon; no respawn, no
 * loot.
 * <p>
 * Java: data/handlers/ai/siege/SiegeCannonAI.java, @AIName("siege_cannon") (the marker is in the .cpp).
 *
 * @author Whoop
 */
class SiegeCannonAI : public NpcAI {
public:
	explicit SiegeCannonAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

	/** Java: final; the skill id in the high bits and the level in the low byte, 0 for a cannon without a morph */
	int32_t getMorphSkill();

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::siege
