#include "aion/gameserver/handlers/ai/instance/abyssal_splinter/DayshadeAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai::instance::abyssal_splinter {

AION_AI(DayshadeAI, "dayshade");

// Java DayshadeAI.java:25-34
void DayshadeAI::handleAttack(runtime::Ptr<Creature> creature) {
	AggressiveNpcAI::handleAttack(creature);
	if (isHome.compareAndSet(true, false)) {
		AIActions::die(*this, *creature);
		spawn(216949, 455.5502f, 702.09485f, 433.13727f, int8_t{108}); // ebonsoul
		spawn(216948, 447.1937f, 683.72217f, 433.1805f, int8_t{108}); // rukril
		AIActions::deleteOwner(*this);
	}
}

// Java DayshadeAI.java:36-40
void DayshadeAI::handleBackHome() {
	AggressiveNpcAI::handleBackHome();
	isHome.set(true);
}

// Java DayshadeAI.java:42-48
bool DayshadeAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_LOOT:
		case AIQuestion::REWARD_AP:
			return false;
		default:
			return AggressiveNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::abyssal_splinter
