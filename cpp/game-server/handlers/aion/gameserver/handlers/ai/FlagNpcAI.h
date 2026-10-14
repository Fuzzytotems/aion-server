#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a flag ("flag": territory markers, gates, banners and the test flags of npc_templates.xml): it takes no damage.
 * <p>
 * Java: data/handlers/ai/FlagNpcAI.java, @AIName("flag").
 *
 * @author Cheatkiller, Sykra
 */
class FlagNpcAI : public NpcAI {
public:
	explicit FlagNpcAI(Npc& owner) : NpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai
