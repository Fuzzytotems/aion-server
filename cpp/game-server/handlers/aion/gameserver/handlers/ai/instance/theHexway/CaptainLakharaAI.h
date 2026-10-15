#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::theHexway {

/**
 * The AI of Captain Lakhara in the Hexway ("captain_lakhara"): at each summon percentage she casts 17497 and summons the group with a smoke
 * each; 25 seconds later the summons go.
 * <p>
 * Java: data/handlers/ai/instance/theHexway/CaptainLakharaAI.java, @AIName("captain_lakhara"). C++: the despawn task is pinned to this AI and
 * holds the static data percentage.
 *
 * @author Sykra
 */
class CaptainLakharaAI : public SummonerAI {
private:
	runtime::Field<runtime::FutureRef> despawnTask{};

public:
	explicit CaptainLakharaAI(Npc& owner) : SummonerAI(owner) {}

protected:
	void handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent) override;

	void handleDespawned() override;

	void handleBackHome() override;

	void handleDied() override;

private:
	void cancelDespawnTask();

	void despawnNpc(int32_t npcId);

	void summonNpcWithSmoke(const model::templates::ai::SummonGroup& summon);
};

} // namespace aion::gameserver::handlers::ai::instance::theHexway
