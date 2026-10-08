#include "aion/gameserver/handlers/ai/portals/StudioPortalAI.h"

#include <optional>

#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::portals {

AION_AI(StudioPortalAI, "studioportal");

template <class T>
T StudioPortalAI::unbox(const std::optional<T>& value) {
	if (!value)
		throw runtime::NullPointerException("null exit attribute of the studio address");
	return *value;
}

// Java StudioPortalAI.java:31-34
bool StudioPortalAI::onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) {
	static_cast<void>(player);
	static_cast<void>(dialogActionId);
	static_cast<void>(questId);
	static_cast<void>(extendedRewardIndex);
	return true;
}

// Java StudioPortalAI.java:36-61
void StudioPortalAI::handleUseItemFinish(Player& player) {
	runtime::Ptr<world::WorldMapInstance> instance;
	float x, y, z;
	int8_t heading = 0;
	const std::optional<world::WorldMapType> mapType = world::getWorldMapType(player.getWorldId());
	if (mapType == world::WorldMapType::HOUSING_IDLF_PERSONAL || mapType == world::WorldMapType::HOUSING_IDDF_PERSONAL) { // leaving studio
		const runtime::Ptr<model::house::House> studio =
			services::HousingService::getInstance().getPlayerStudio(player.getPosition()->getWorldMapInstance()->getOwnerId());
		if (!studio) // should not happen unless this instance was custom spawned by admin
			return;
		instance = world::World::getInstance().getWorldMap(unbox(studio->getAddress()->getExitMapId()))->getMainWorldMapInstance();
		x = unbox(studio->getAddress()->getExitX());
		y = unbox(studio->getAddress()->getExitY());
		z = unbox(studio->getAddress()->getExitZ());
	} else { // entering own studio
		const runtime::Ptr<model::house::House> studio = services::HousingService::getInstance().getPlayerStudio(player.getObjectId());
		if (!studio) { // doesn't own studio
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_HOUSING_ENTER_NEED_HOUSE());
			return;
		}
		instance = services::instance::InstanceService::getOrCreateHouseInstance(*studio);
		x = studio->getAddress()->getX();
		y = studio->getAddress()->getY();
		z = studio->getAddress()->getZ();
		heading = studio->getTeleportHeading();
	}
	services::teleport::TeleportService::teleportTo(player, *instance, x, y, z, heading, model::animations::TeleportAnimation::FADE_OUT_BEAM);
}

} // namespace aion::gameserver::handlers::ai::portals
