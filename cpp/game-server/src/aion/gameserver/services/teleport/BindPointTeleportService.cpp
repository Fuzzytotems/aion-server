#include "aion/gameserver/services/teleport/BindPointTeleportService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::teleport {

BindPointTeleportService::Cooldown::Cooldown(int32_t value, int64_t cdEndValue) : locId(value), cdEnd(cdEndValue) {
}

runtime::Ref<BindPointTeleportService::Cooldown> BindPointTeleportService::Cooldown::create(int32_t value, int64_t cdEndValue) {
	return runtime::makeRef<BindPointTeleportService::Cooldown>(value, cdEndValue);
}

int32_t BindPointTeleportService::Cooldown::getTimeLeft() {
	AION_UNPORTED();
}

BindPointTeleportService::Cooldown::~Cooldown() = default;

void BindPointTeleportService::onLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

// anonymous Runnable at BindPointTeleportService.java:53 (fieldmap key BindPointTeleportService$1); argument 1 of schedule(); storage: task
// anonymous Runnable at BindPointTeleportService.java:63 (fieldmap key BindPointTeleportService$2); argument 1 of schedule(); storage: task
void BindPointTeleportService::teleport(model::gameobjects::player::Player& player, int32_t locId, int64_t kinah) {
	AION_UNPORTED();
}

void BindPointTeleportService::cancelTeleport(model::gameobjects::player::Player& player, int32_t locId) {
	AION_UNPORTED();
}

int64_t BindPointTeleportService::calculateTeleportationPrice(model::gameobjects::player::Player& player, const model::templates::hotspot::HotspotTemplate* hotspot, int64_t priceSentByGameClient) {
	AION_UNPORTED();
}

bool BindPointTeleportService::checkRequirements(model::gameobjects::player::Player& player, const model::templates::hotspot::HotspotTemplate* hotspot, int64_t price) {
	AION_UNPORTED();
}

void BindPointTeleportService::addCooldown(model::gameobjects::player::Player& player, int32_t locId) {
	AION_UNPORTED();
}

runtime::Ptr<BindPointTeleportService::Cooldown> BindPointTeleportService::getCooldown(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::teleport
