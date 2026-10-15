#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/model/templates/ai/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a summoner ("summoner": Reshanta's summoner monsters, Haramel's boss): at the hp percentages of its ai_templates <summons> it
 * casts a skill and calls its helpers (each summon group after its schedule); they leave when it dies, despawns or returns home.
 * <p>
 * Java: data/handlers/ai/SummonerAI.java, @AIName("summoner"); the instance AIs extend it through the protected hooks. Java's
 * `percentage = Collections.emptyList()` before the spawn is a null list here (empty for iteration; clear() of it is Java's
 * UnsupportedOperationException, thrown explicitly).
 */
class SummonerAI : public AggressiveNpcAI {
public:
	explicit SummonerAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;
	void handleDespawned() override;
	void handleBackHome() override;
	void handleNotAtHome() override;
	void handleSpawned() override;
	void handleDied() override;

	void addHelpersSpawn(int32_t objId);
	virtual void spawnHelpers(const model::templates::ai::SummonGroup& summonGroup);
	virtual bool checkBeforeSpawn();
	virtual void handleBeforeSpawn(const model::templates::ai::Percentage& percent);
	virtual void handleSpawnFinished(const model::templates::ai::SummonGroup& summonGroup);
	virtual void handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent);

private:
	void removeAndResetHelperSpawns();
	void clearPercentage();
	void checkPercentage(int32_t hpPercentage);

	/** Java: private final List<Integer> spawnedNpc = new ArrayList<>(), guarded by synchronized (spawnedNpc) */
	runtime::ArrayList<int32_t> spawnedNpc{AION_LOCK_CLASS(SummonerAI::spawnedNpc)};
	/** Java: private List<Percentage> percentage = Collections.emptyList() (null here) */
	runtime::Field<runtime::Ref<runtime::RcArrayList<const model::templates::ai::Percentage*>>> percentage{};
	/** Java: private volatile int spawnedPercent = 0 */
	runtime::Field<int32_t> spawnedPercent{0};
};

} // namespace aion::gameserver::handlers::ai
