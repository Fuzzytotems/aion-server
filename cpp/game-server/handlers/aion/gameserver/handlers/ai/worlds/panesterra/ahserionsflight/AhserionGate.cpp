#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionGate.h"

#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionGate, "ahserion_gate");

// Java AhserionGate.java:20-24
void AhserionGate::handleSpawned() {
	AhserionConstructAI::handleSpawned();
	useBuff();
}

// Java AhserionGate.java:26-29
void AhserionGate::useBuff() {
	SkillEngine::getInstance().applyEffectDirectly(21515, getSkillLevel(), getOwner(), getOwner(), std::nullopt,
		skillengine::model::Effect_ForceType::DEFAULT);
	getOwner().setTarget(nullptr);
}

// Java AhserionGate.java:31-42
int32_t AhserionGate::getSkillLevel() {
	switch (getNpcId()) {
		case 277229:
			return 40; // Hangar Barricade
		case 277230:
			return 50; // Ahserion's Flight Barrier
		case 277231:
			return 60; // Bulwark Shield
		default:
			return 0;
	}
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
