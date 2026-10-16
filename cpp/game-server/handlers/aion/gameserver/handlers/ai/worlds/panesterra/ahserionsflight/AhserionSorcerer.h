#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionAggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

/**
 * An Ahserion sorcerer ("ahserion_sorcerer"): its attacks are magical fire.
 * <p>
 * Java: data/handlers/ai/worlds/panesterra/ahserionsflight/AhserionSorcerer.java, @AIName("ahserion_sorcerer") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class AhserionSorcerer : public AhserionAggressiveNpcAI {
public:
	explicit AhserionSorcerer(Npc& owner) : AhserionAggressiveNpcAI(owner) {}

	ItemAttackType modifyAttackType(ItemAttackType type) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
