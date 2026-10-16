#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <vector>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

/**
 * The AI of Yamennes in the Abyssal Splinter ("yamennes"): the first attack starts the 10 minute enrage and the portal round - every
 * minute the three gates (when none is left, alternating below and above) and 3 s later the healing debuff (her aggro reset with three
 * spheres); back home she goes; no loot, no AP.
 * <p>
 * Java: data/handlers/ai/instance/abyssal_splinter/YamennesAI.java, @AIName("yamennes") (the marker is in the .cpp).
 *
 * @author Ritsu, Luzien
 */
class YamennesAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::FutureRef> portalTask{};
	runtime::Field<runtime::FutureRef> enrageTask{};
	runtime::AtomicBoolean isStart{AION_LOCK_CLASS(YamennesAI::isStart)}; // Java: = new AtomicBoolean()

public:
	explicit YamennesAI(Npc& owner) : AggressiveNpcAI(owner) {}

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

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
