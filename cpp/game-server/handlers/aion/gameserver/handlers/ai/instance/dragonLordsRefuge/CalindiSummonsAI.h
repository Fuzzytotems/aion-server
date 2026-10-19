#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of Calindi's flames and blazes ("calindi_summon"): with its fake texture npc, from 0.5 s its skill every 0.5 s (flames) or
 * 2 s (blazes); gone after 15 s with its texture; no decay, respawn or rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/CalindiSummonsAI.java, @AIName("calindi_summon") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class CalindiSummonsAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> task{};
	runtime::Field<runtime::Ref<VisibleObject>> textureObject{}; // Java: = null

public:
	explicit CalindiSummonsAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	int32_t getSkillId();

	int32_t getFakeTextureNpcId();

	int32_t getDelay();

public:
	void handleDespawned() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
