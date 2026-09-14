#include "aion/gameserver/services/PunishmentService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.PunishmentService@L109:46
//   com.aionemu.gameserver.services.PunishmentService@L121:90

void PunishmentService::unbanChar(int32_t playerId) {
	AION_UNPORTED();
}

void PunishmentService::banChar(int32_t playerId, int32_t dayCount, std::string_view reason) {
	AION_UNPORTED();
}

int64_t PunishmentService::calculateDuration(int32_t dayCount) {
	AION_UNPORTED();
}

void PunishmentService::setIsInPrison(model::gameobjects::player::Player& player, bool state, int64_t delayInMinutes, std::string_view reason) {
	AION_UNPORTED();
}

void PunishmentService::updatePrisonStatus(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PunishmentService::schedulePrisonTask(model::gameobjects::player::Player& player, int64_t prisonTimer) {
	AION_UNPORTED();
}

void PunishmentService::setIsNotGatherable(model::gameobjects::player::Player& player, int32_t captchaCount, bool state, int64_t delay) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
