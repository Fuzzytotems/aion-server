#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

/**
 * The AI of Moriata in Sauro Supply Base ("moriata"): from her first hit every 12 s she checks for kiting - 10 s without an attack or skill
 * speed her up and enrage her, every third time for good; back home, dead or despawned she stops.
 * <p>
 * Java: data/handlers/ai/instance/sauroBase/MoriataAI.java, @AIName("moriata") (the marker is in the .cpp).
 *
 * @author Estrayl March 22nd, 2018
 */
class MoriataAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean hasAggro{AION_LOCK_CLASS(MoriataAI::hasAggro)}; // Java: = new AtomicBoolean()
	runtime::Field<runtime::FutureRef> task{};
	runtime::Field<int64_t> lastSkillUse{};
	runtime::Field<int32_t> kiteCount{};

public:
	explicit MoriataAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void observeKiting();

protected:
	void handleAttackComplete() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void cancelTask();

protected:
	void handleBackHome() override;

	void handleDied() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
