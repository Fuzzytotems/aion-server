#include "aion/gameserver/handlers/ai/instance/tiamatStrongHold/AdjutantAnuhartAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

AION_AI(AdjutantAnuhartAI, "adjutantanuhart");

// Java AdjutantAnuhartAI.java:26-30
void AdjutantAnuhartAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	hpPhases->tryEnterNextPhase(*this);
}

// Java AdjutantAnuhartAI.java:32-39
void AdjutantAnuhartAI::onStartUseSkill(const SkillTemplate* skillTemplate, int32_t /*skillLevel*/) {
	if (skillTemplate->getSkillId() == 20747) { // Blade Storm
		SkillEngine::getInstance().applyEffect(20749, getOwner(), getOwner());
		getEffectController()->setAbnormal(skillengine::effect::AbnormalState::SANCTUARY);
		spawn(283099, getOwner().getX(), getOwner().getY(), getOwner().getZ(), int8_t{0});
	}
}

// Java AdjutantAnuhartAI.java:41-45
void AdjutantAnuhartAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t /*skillLevel*/) {
	if (skillTemplate->getSkillId() == 20747) // Blade Storm
		getEffectController()->unsetAbnormal(skillengine::effect::AbnormalState::SANCTUARY);
}

// Java AdjutantAnuhartAI.java:47-54
void AdjutantAnuhartAI::handleHpPhase(int32_t phaseHpPercent) {
	switch (phaseHpPercent) {
		case 50:
			useSelfBuff(20938);
			break;
		case 25:
			useSelfBuff(20939);
			break;
		case 10:
			useSelfBuff(20940);
			break;
		default:
			break;
	}
}

// Java AdjutantAnuhartAI.java:56-59
void AdjutantAnuhartAI::useSelfBuff(int32_t buffSkillId) {
	AIActions::targetSelf(*this);
	AIActions::useSkill(*this, buffSkillId);
}

// Java AdjutantAnuhartAI.java:61-65
void AdjutantAnuhartAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	hpPhases->reset();
}

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
