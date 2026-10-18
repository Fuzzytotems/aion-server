#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

/**
 * Dynatoum, the Illuminary Obelisk's end boss ("dynatoum"): its ten-minute timer from the first attack, the portals gone at 70 %, and its
 * self-destruction at the end of the time.
 * <p>
 * Java: data/handlers/ai/instance/illuminaryObelisk/DynatoumAI.java, @AIName("dynatoum") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class DynatoumAI : public SummonerAI {
private:
	runtime::AtomicBoolean isStarted{AION_LOCK_CLASS(DynatoumAI::isStarted)}; // Java: = new AtomicBoolean()
	runtime::AtomicBoolean areEntriesRemoved{AION_LOCK_CLASS(DynatoumAI::areEntriesRemoved)}; // Java: = new AtomicBoolean()

protected:
	runtime::Field<runtime::FutureRef> despawnTask{};

public:
	explicit DynatoumAI(Npc& owner) : SummonerAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

	virtual void removeBossEntries();

	virtual void scheduleDespawn(int32_t delayInSec);

public:
	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void cancelDespawnTask();

	void handleDespawned() override;

	void handleDied() override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
