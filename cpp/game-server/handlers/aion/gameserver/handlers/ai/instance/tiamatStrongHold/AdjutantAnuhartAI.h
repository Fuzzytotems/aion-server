#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Adjutant Anuhart in Tiamat Stronghold ("adjutantanuhart"): starting Blade Storm (20747) he puts 20749 on himself, becomes a
 * sanctuary and calls a blade storm (283099), ending it he leaves the sanctuary; at 50, 25 and 10 % HP he casts 20938, 20939 and 20940 on
 * himself; back home his phases start over.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/AdjutantAnuhartAI.java, @AIName("adjutantanuhart") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Estrayl
 */
class AdjutantAnuhartAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(50, {25, 10}); // Java: = new HpPhases(50, 25, 10)

public:
	explicit AdjutantAnuhartAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void useSelfBuff(int32_t buffSkillId);

protected:
	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
