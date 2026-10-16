#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::pvpArenas {

/**
 * The AI of the arenas' anti-aircraft guns ("antiaircraftgun"): usable once the run started; the use puts the player on the gun, morphs him
 * by the gun's level and respawns the gun.
 * <p>
 * Java: data/handlers/ai/instance/pvpArenas/AntiAirCraftGunAI.java, @AIName("antiaircraftgun") (the marker is in the .cpp).
 *
 * @author xTz
 */
class AntiAirCraftGunAI : public ActionItemNpcAI {
public:
	explicit AntiAirCraftGunAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::pvpArenas
