#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of Orissan's crystals ("orissans_summon"): hit by another npc it takes 10000 and a second later (once) casts: 855699 its 21638
 * on itself, the other 21639 on the attacker; a cast kills it; it never goes home; immune to abnormal states; no rewards.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/OrissansSummonAI.java, @AIName("orissans_summon") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class OrissansSummonAI : public GeneralNpcAI {
private:
	runtime::AtomicBoolean isActive{AION_LOCK_CLASS(OrissansSummonAI::isActive)}; // Java: = new AtomicBoolean()

public:
	explicit OrissansSummonAI(Npc& owner) : GeneralNpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleBackHome() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
