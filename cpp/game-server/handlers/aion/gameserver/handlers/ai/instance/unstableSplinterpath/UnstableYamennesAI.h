#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <vector>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

/**
 * The AI of Yamennes in the Unstable Splinterpath ("unstableyamennes"): the first attack starts the 10 minute enrage and the portal round - every
 * minute the three gates (when none is left, alternating below and above) and 3 s later the healing debuff (her aggro reset with three
 * spheres); back home she goes; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/unstableSplinterpath/UnstableYamennesAI.java, @AIName("unstableyamennes") (the marker is in the .cpp).
 *
 * @author Ritsu, Luzien, Cheatkiller
 */
class UnstableYamennesAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::FutureRef> portalTask{};
	runtime::Field<runtime::FutureRef> enrageTask{};
	runtime::AtomicBoolean isStart{AION_LOCK_CLASS(UnstableYamennesAI::isStart)}; // Java: = new AtomicBoolean()

public:
	explicit UnstableYamennesAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void startTasks();

	void onHealingDebuff();

	void spawnPortals(bool isTopSpawn);

	void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);

	void cancelTasks();

protected:
	void handleBackHome() override;

	void handleDespawned() override;

	void handleDied() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
