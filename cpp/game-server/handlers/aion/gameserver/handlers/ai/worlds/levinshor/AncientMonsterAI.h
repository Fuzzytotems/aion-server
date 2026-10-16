#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::worlds::levinshor {

/**
 * The AI of the ancient monsters of Levinshor's advance ("LDF4_Advance_Ancient_Monster", the bosses a Vocolith calls): deleted an hour after
 * the spawn unless dead; arrived more than 15 m from its spawn it gives up its target; 1.5 times its damage, 3 times with 21780.
 * <p>
 * Java: data/handlers/ai/worlds/levinshor/AncientMonsterAI.java, @AIName("LDF4_Advance_Ancient_Monster") (the marker is in the .cpp).
 *
 * @author Yeats
 */
class AncientMonsterAI : public AggressiveNpcAI {
public:
	explicit AncientMonsterAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleMoveArrived() override;

public:
	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;
};

} // namespace aion::gameserver::handlers::ai::worlds::levinshor
