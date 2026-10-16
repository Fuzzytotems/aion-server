#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

/**
 * The AI of Ashunatal Shadowslip ("ashunatal_shadowslip"): her aggro closes door 2, her return home opens it; her death opens doors 17
 * and 2; after her shadow skill (19417) she walks off and goes.
 * <p>
 * Java: data/handlers/ai/instance/aturamSkyFortress/AshunatalShadowslipAI.java, @AIName("ashunatal_shadowslip") (the marker is in the .cpp).
 *
 * @author xTz
 */
class AshunatalShadowslipAI : public AggressiveNpcAI {
private:
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(AshunatalShadowslipAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<bool> canThink_{true}; // Java: canThink (the name clashes with canThink())

public:
	explicit AshunatalShadowslipAI(Npc& owner) : AggressiveNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleCreatureAggro(Creature& creature) override;

	void handleBackHome() override;

	void handleDied() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
