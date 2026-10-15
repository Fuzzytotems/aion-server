#include "aion/gameserver/handlers/ai/instance/unstableSplinterpath/UnstableDayshadeAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath {

AION_AI(UnstableDayshadeAI, "unstabledayshade");

// Java UnstableDayshadeAI.java:25-34
void UnstableDayshadeAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false)) {
		AIActions::die(*this, *creature);
		spawn(219552, 455.5502f, 702.09485f, 433.13727f, int8_t{108}); // ebonsoul
		spawn(219551, 447.1937f, 683.72217f, 433.1805f, int8_t{108}); // rukril
		AIActions::deleteOwner(*this);
	}
}

// Java UnstableDayshadeAI.java:36-40
void UnstableDayshadeAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	isHome.set(true);
}

// Java UnstableDayshadeAI.java:42-48
bool UnstableDayshadeAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::unstableSplinterpath
