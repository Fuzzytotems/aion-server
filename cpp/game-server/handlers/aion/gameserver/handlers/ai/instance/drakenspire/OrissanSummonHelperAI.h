#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of the crystals' summon helpers ("orissan_summon_helper"): it takes no damage; a second after its spawn it casts 21647 on
 * itself, 1.5 s after a cast it becomes its summon (855699 for 855607, else 855700) and goes; it never goes home; no rewards.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/OrissanSummonHelperAI.java, @AIName("orissan_summon_helper") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class OrissanSummonHelperAI : public GeneralNpcAI {
public:
	explicit OrissanSummonHelperAI(Npc& owner) : GeneralNpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

protected:
	void handleSpawned() override;

private:
	void handleSkillTask();

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleBackHome() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
