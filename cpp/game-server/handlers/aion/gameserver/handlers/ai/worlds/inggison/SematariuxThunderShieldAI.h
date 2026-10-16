#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds::inggison {

/**
 * The AI of Sematariux's thunder shields in Inggison ("sematariux_thunder_shield"): a tenth of their max HP and half their damage.
 * <p>
 * Java: data/handlers/ai/worlds/inggison/SematariuxThunderShieldAI.java, @AIName("sematariux_thunder_shield") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class SematariuxThunderShieldAI : public AggressiveNpcAI {
public:
	explicit SematariuxThunderShieldAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void modifyOwnerStat(Stat2& stat) override;

	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::inggison
