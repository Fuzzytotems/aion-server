#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionSorcerer.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionSorcerer, "ahserion_sorcerer");

// Java AhserionSorcerer.java:17-20
ItemAttackType AhserionSorcerer::modifyAttackType(ItemAttackType type) {
	static_cast<void>(type);
	return ItemAttackType::MAGICAL_FIRE;
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
