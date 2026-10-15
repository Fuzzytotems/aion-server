#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <vector>

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of Kuhara the Volatile in Rentus Base ("kuhara_the_volatile"): from the first attack his 14 s burst and his 50 s barrel round -
 * two barrels at one of four places and, 14 s later, his bomb event (he stops, eight bombs walk to him, 11 s later he fights on and the
 * barrels and bombs go); back home, despawned or dead he stops. (Java: still not retail-like and fully refactored.)
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/KuharaTheVolatileAI.java, @AIName("kuhara_the_volatile") (the marker is in the .cpp).
 *
 * @author xTz, Estrayl
 */
class KuharaTheVolatileAI : public AggressiveNpcAI {
private:
	runtime::Field<runtime::FutureRef> activeEventTask{};
	runtime::Field<runtime::FutureRef> barrelEventTask{};
	runtime::Field<runtime::FutureRef> bombEventTask{};
	runtime::AtomicBoolean isStarted{AION_LOCK_CLASS(KuharaTheVolatileAI::isStarted)}; // Java: = new AtomicBoolean()
	runtime::Field<bool> canThink_{true}; // Java: canThink (the name clashes with canThink())

public:
	explicit KuharaTheVolatileAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void cancelTask(runtime::FutureRef task);

	void startBarrelEvent();

	void startBombEvent();

	void deleteNpcs(const std::vector<runtime::Ptr<Npc>>& npcs);

	void spawnBombEvent();

	void moveBombToBoss(runtime::Ptr<Npc> npc);

	runtime::Ptr<Npc> rndSpawn(int32_t npcId, float x, float y, float z);

	void startActiveEvent();

protected:
	void handleDespawned() override;

	void handleBackHome() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
