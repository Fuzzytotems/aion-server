#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/DorakikiTheBoldAI.h"

#include "aion/gameserver/controllers/effect/EffectController.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(DorakikiTheBoldAI, "dorakiki_the_bold");

// Java DorakikiTheBoldAI.java:18-24
void DorakikiTheBoldAI::handleAttackComplete() {
	AggressiveNpcAI::handleAttackComplete();
	if (getEffectController()->hasAbnormalEffect(18901)) {
		getEffectController()->removeEffect(18901);
	}
}

// Java DorakikiTheBoldAI.java:26-32
void DorakikiTheBoldAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	if (getEffectController()->hasAbnormalEffect(18901)) {
		getEffectController()->removeEffect(18901);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
