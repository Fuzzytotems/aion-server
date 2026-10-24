#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of Chief Gunner Koakoa on the Steel Rake ("gunnerkoakoa"): each individual summons percentage calls one of three gunner layouts
 * (281212 and 281213 on fifteen places); under 18552 that effect is taken off 21 s later.
 * <p>
 * Java: data/handlers/ai/instance/rakes/ChiefGunnerKoakoaAI.java, @AIName("gunnerkoakoa") (the marker is in the .cpp).
 *
 * @author xTz
 */
class ChiefGunnerKoakoaAI : public SummonerAI {
public:
	explicit ChiefGunnerKoakoaAI(Npc& owner) : SummonerAI(owner) {}

protected:
	void handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent) override;

private:
	void checkAbnormalEffect();

	void randomSpawn(int32_t i);
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
