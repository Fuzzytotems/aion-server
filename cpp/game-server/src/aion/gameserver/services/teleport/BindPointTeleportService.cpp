#include "aion/gameserver/services/teleport/BindPointTeleportService.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_TELEPORT.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::teleport {

BindPointTeleportService::Cooldown::Cooldown(int32_t value, int64_t cdEndValue) : locId(value), cdEnd(cdEndValue) {
}

runtime::Ref<BindPointTeleportService::Cooldown> BindPointTeleportService::Cooldown::create(int32_t value, int64_t cdEndValue) {
	return runtime::makeRef<BindPointTeleportService::Cooldown>(value, cdEndValue);
}

int32_t BindPointTeleportService::Cooldown::getTimeLeft() {
	int32_t estimated = static_cast<int32_t>((cdEnd.get() - commons::utils::currentTimeMillis()) / 1000);
	if (estimated > 0)
		return estimated;
	else
		return 0;
}

BindPointTeleportService::Cooldown::~Cooldown() = default;

void BindPointTeleportService::onLogin(model::gameobjects::player::Player& player) {
	runtime::Ptr<Cooldown> cooldown = getCooldown(player);
	if (cooldown && cooldown->getTimeLeft() > 0)
		utils::PacketSendUtility::broadcastPacketAndReceive(player, network::aion::serverpackets::SM_BIND_POINT_TELEPORT(3, player.getObjectId(),
			cooldown->getLocId(), cooldown->getTimeLeft()));
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
	int64_t cooldown = commons::utils::currentTimeMillis() + COOLDOWN_IN_SECONDS * 1000;
	cooldowns.put(player.getObjectId(), Cooldown::create(locId, cooldown));
}

runtime::Ptr<BindPointTeleportService::Cooldown> BindPointTeleportService::getCooldown(model::gameobjects::player::Player& player) {
	return cooldowns.get(player.getObjectId());
}

} // namespace aion::gameserver::services::teleport
