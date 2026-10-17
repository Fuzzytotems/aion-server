#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of the big orb ("bigorb"): while no portal (730276) is out, its dialog offers one and the first choice brings it.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/BigOrbAI.java, @AIName("bigorb") (the marker is in the .cpp).
 *
 * @author unknown
 */
class BigOrbAI : public NpcAI {
public:
	explicit BigOrbAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

public:
	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

private:
	bool isPortalSpawned();
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
