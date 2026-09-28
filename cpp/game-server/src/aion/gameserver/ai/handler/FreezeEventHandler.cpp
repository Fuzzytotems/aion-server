#include "aion/gameserver/ai/handler/FreezeEventHandler.h"

#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"

namespace aion::gameserver::ai::handler {

void FreezeEventHandler::onUnfreeze(AbstractAI& ai) {
	if (ai.isInSubState(AISubState::FREEZE)) {
		ai.setSubStateIfNot(AISubState::NONE);
		ai.think();
	}
}

void FreezeEventHandler::onFreeze(AbstractAI& ai) {
	ai.setSubStateIfNot(AISubState::FREEZE);
	ai.think();
}

} // namespace aion::gameserver::ai::handler
