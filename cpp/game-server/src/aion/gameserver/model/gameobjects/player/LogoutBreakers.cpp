#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player {

// The bodies need the hub headers of the cut members and the C++-only breaker methods requested from their groups
// (VisibleObject::breakTarget, ObserveController::clearWithoutNotify, PlayerController::breakStanceObserver, IdianStone::breakActionListener,
// EffectController::clearEffectMapsWithoutNotify, CreatureGameStats::clearEffectFunctionsWithoutNotify); they are ported together with
// PlayerLeaveWorldService.leaveWorld (P5-00 login slice).

void LogoutBreakers::run(Player& player) noexcept {
	try {
		AION_UNPORTED();
	} catch (const runtime::UnportedException&) {
		// noexcept (class comment): a failing step is logged (AION_UNPORTED logs its first hit) and never terminates the caller
	}
}

void LogoutBreakers::onDelete(VisibleObject& object) noexcept {
	try {
		AION_UNPORTED();
	} catch (const runtime::UnportedException&) {
		// noexcept (class comment): a failing step is logged (AION_UNPORTED logs its first hit) and never terminates the caller
	}
}

std::vector<const char*> LogoutBreakers::breakZombieEdges(VisibleObject& object) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
