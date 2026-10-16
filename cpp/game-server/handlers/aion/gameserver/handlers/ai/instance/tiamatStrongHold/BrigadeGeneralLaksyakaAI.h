#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Brigade General Laksyaka in Tiamat Stronghold ("brigadegenerallaksyaka"): a peace npc until spawned so; each attack has a 3 %
 * chance to call four skeletons (283115) 10 m around her while none stands; from her first attack, every 40 s (from 5 s), Tiamat's Eye (283089)
 * puts 20865 on a random living player within 40 m of it; at 25 % HP she casts 20731 once (again after a return home).
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralLaksyakaAI.java, @AIName("brigadegenerallaksyaka") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class BrigadeGeneralLaksyakaAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(BrigadeGeneralLaksyakaAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> skeletonTask{};
	runtime::Field<bool> isFinalBuff{};

public:
	explicit BrigadeGeneralLaksyakaAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void startSkillTask();

	void cancelTask();

	void startSkeletonEvent();

	void spawnSummon();

	void rndSpawn(int32_t npcId, int32_t count);

protected:
	void handleDied() override;

	void handleBeforeSpawned() override;

	void handleDespawned() override;

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
