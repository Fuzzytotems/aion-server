#include "aion/gameserver/handlers/ai/InvisiblekiskAI.h"

#include "aion/gameserver/ai/AIActions.h"

namespace aion::gameserver::handlers::ai {

AION_AI(InvisiblekiskAI, "invisible_kisk");

// Java InvisiblekiskAI.java:20-24
void InvisiblekiskAI::handleSpawned() {
	KiskAI::handleSpawned();
	AIActions::useSkill(*this, getHideSkillId());
}

// Java InvisiblekiskAI.java:26-36
int32_t InvisiblekiskAI::getHideSkillId() {
	switch (getNpcId()) {
		case 701768:
		case 701770:
			return 21261;
		case 701769:
		case 701771:
			return 21262;
	}
	return 0;
}

} // namespace aion::gameserver::handlers::ai
