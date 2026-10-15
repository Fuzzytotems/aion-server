#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of Vasharti's assassins in Rentus Base ("vasharti_assassin"): they hide with 19915 2 s after their spawn; the first aggro
 * leaves a smoke (gone at once); back home they hide again.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/VashartiAssassinAI.java, @AIName("vasharti_assassin") (the marker is in the .cpp).
 *
 * @author xTz
 */
class VashartiAssassinAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(VashartiAssassinAI::isHome), true}; // Java: = new AtomicBoolean(true)

public:
	explicit VashartiAssassinAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleCreatureAggro(Creature& creature) override;

	void handleSpawned() override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
