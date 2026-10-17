#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/drakenspire/Lv1HumanBeritraAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of Beritra's human form, level 2 ("drakenspire_lv2_human_beritra"): no helpers at the start; each blasted Dragon Lord Seal
 * (21624) brings pustules, the third ends the seals; his level-58 soul extinction field starts the third phase, which brings the helpers
 * while 21612 lasts.
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/Lv2HumanBeritraAI.java, @AIName("drakenspire_lv2_human_beritra") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class Lv2HumanBeritraAI : public Lv1HumanBeritraAI {
private:
	runtime::AtomicInteger sealsBlasted{AION_LOCK_CLASS(Lv2HumanBeritraAI::sealsBlasted)}; // Java: = new AtomicInteger()

public:
	explicit Lv2HumanBeritraAI(Npc& owner) : Lv1HumanBeritraAI(owner) {}

protected:
	void handleFightStarted() override;

	virtual void handleThirdPhaseStarted();

	virtual void handleLastSealBlasted();

public:
	void onEffectApplied(Effect& effect) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void spawnPustules();

protected:
	void resetBlastedSeal();

	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
