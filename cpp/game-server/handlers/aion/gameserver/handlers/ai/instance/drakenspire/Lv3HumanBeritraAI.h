#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/Lv2HumanBeritraAI.h"

#include "aion/gameserver/ai/HpPhases.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of Beritra's human form, level 3 ("drakenspire_lv3_human_beritra"): the last seal or 40 % hp queue his Dragon Lord's Authority
 * (20842), which hits every player of the map and turns him into the dragon (236247) after the ceiling collapses; the third phase renews his
 * lost buffs, heals him and resets the seals.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/Lv3HumanBeritraAI.java, @AIName("drakenspire_lv3_human_beritra") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class Lv3HumanBeritraAI : public Lv2HumanBeritraAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(40); // Java: = new HpPhases(40)

public:
	explicit Lv3HumanBeritraAI(Npc& owner) : Lv2HumanBeritraAI(owner) {}

protected:
	void handleLastSealBlasted() override;

	void handleThirdPhaseStarted() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void handleTransformation();

public:
	void handleHpPhase(int32_t phaseHpPercent) override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
