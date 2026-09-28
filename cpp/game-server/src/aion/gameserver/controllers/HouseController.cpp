#include "aion/gameserver/controllers/HouseController.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseNpcsData.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/SummonedHouseNpc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"
#include "aion/gameserver/model/templates/spawns/HouseSpawn.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::controllers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.controllers.HouseController");

using model::animations::TeleportAnimation;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::house::House;
using model::templates::housing::HouseAddress;
using model::templates::spawns::SpawnTemplate;
using model::templates::spawns::SpawnType;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;

HouseController::HouseController() = default;

HouseController::~HouseController() = default;

model::house::House& HouseController::getOwner() const {
	return static_cast<model::house::House&>(VisibleObjectController::getOwner());
}

void HouseController::see(model::gameobjects::VisibleObject& object) {
	if (runtime::as<Player>(object))
		spawnObjects();
}

void HouseController::spawnObjects() {
	if (getOwner().getPosition() && getOwner().isSpawned() && !getOwner().isInactive()) {
		for (const Ptr<model::gameobjects::HouseObject>& obj : getOwner().getRegistry()->getSpawnedObjects())
			obj->spawn();
	}
}

void HouseController::onAfterSpawn() {
	// loads scripts and registry from DB if not already initialized
	getOwner().getPlayerScripts();
	getOwner().getRegistry();
	updateSpawns();
	world::geo::GeoService::getInstance().setHouseDoorState(getOwner().getWorldId(), getOwner().getInstanceId(), getOwner().getAddress()->getId(),
		getOwner().getDoorState());
}

void HouseController::updateSpawns() {
	const HouseAddress* address = getOwner().getAddress();
	const std::vector<model::templates::spawns::HouseSpawn>* templates = dataholders::DataManager::HOUSE_NPCS_DATA->getSpawnsByAddress(address->getId());
	if (templates == nullptr) {
		log.warn("Missing npc spawns for house " + std::to_string(address->getId()));
		return;
	}
	for (const model::templates::spawns::HouseSpawn& spawn : *templates) {
		Ref<Npc> npc;
		if (spawn.getType() == SpawnType::MANAGER) {
			Ref<SpawnTemplate> t = spawnengine::SpawnEngine::newSingleTimeSpawn(address->getMapId(), address->getLand()->getManagerNpcId(), spawn.getX(),
				spawn.getY(), spawn.getZ(), spawn.getH());
			npc = spawnengine::VisibleObjectSpawner::spawnHouseNpc(*t, getOwner().getInstanceId(), getOwner());
		} else if (spawn.getType() == SpawnType::TELEPORT) {
			Ref<SpawnTemplate> t = spawnengine::SpawnEngine::newSingleTimeSpawn(address->getMapId(), address->getLand()->getTeleportNpcId(), spawn.getX(),
				spawn.getY(), spawn.getZ(), spawn.getH());
			npc = spawnengine::VisibleObjectSpawner::spawnHouseNpc(*t, getOwner().getInstanceId(), getOwner());
		} else if (spawn.getType() == SpawnType::SIGN) {
			// Signs do not have master name displayed, but have creatorId
			int32_t creatorId = address->getId();
			Ref<SpawnTemplate> t = spawnengine::SpawnEngine::newSingleTimeSpawn(address->getMapId(), getCurrentSignNpcId(), spawn.getX(), spawn.getY(),
				spawn.getZ(), spawn.getH(), creatorId);
			npc = runtime::cast<Npc>(spawnengine::SpawnEngine::spawnObject(*t, getOwner().getInstanceId()));
		} else {
			log.warn(std::string("Unhandled spawn type ").append(xml::enumName(spawn.getType())));
			continue;
		}
		getOwner().updateSpawn(spawn.getType(), npc);
	}
}

void HouseController::onDespawn() {
	VisibleObjectController::onDespawn();
	bool isReusableStudio = services::HousingService::getInstance().findStudio(getOwner().getObjectId()) == Ptr<House>(getOwner());
	if (isReusableStudio) { // save studio and release despawned npcs and the destroyed mapregion / worldmapinstance, since studio stays in RAM
		getOwner().save();
		getOwner().clearSpawns();
		getOwner().setPosition(nullptr);
	}
}

void HouseController::updateAppearance() {
	PacketSendUtility::broadcastPacket(getOwner(), network::aion::serverpackets::SM_HOUSE_UPDATE(getOwner()));
}

void HouseController::kickVisitors(runtime::Ptr<model::gameobjects::player::Player> kicker, bool kickFriends, bool ownerChanged) {
	const world::zone::ZoneName* houseZone = world::zone::ZoneName::get(getOwner().getName());
	getOwner().getKnownList().forEachPlayer([this, &kicker, kickFriends, ownerChanged, houseZone](Player& player) {
		if (player.getObjectId() == getOwner().getOwnerId())
			return;
		if (!kickFriends && kicker && kicker->getFriendList().getFriend(player.getObjectId()))
			return;
		if (player.isInsideZone(houseZone))
			moveOutside(player, ownerChanged);
	});
	if (kicker) {
		if (!kickFriends) {
			PacketSendUtility::sendPacket(*kicker, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_ORDER_OUT_WITHOUT_FRIENDS());
		} else {
			PacketSendUtility::sendPacket(*kicker, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_ORDER_OUT_ALL());
		}
	}
}

void HouseController::moveOutside(model::gameobjects::player::Player& player, bool ownerChanged) {
	if (getOwner().getAddress()->getExitMapId()) {
		const HouseAddress* address = getOwner().getAddress();
		// Java: unboxing of the exit coordinates (NullPointerException if absent)
		services::teleport::TeleportService::teleportTo(player, detail::unbox(address->getExitMapId(), "exitMapId"),
			detail::unbox(address->getExitX(), "exitX"), detail::unbox(address->getExitY(), "exitY"), detail::unbox(address->getExitZ(), "exitZ"), int8_t{0},
			TeleportAnimation::FADE_OUT_BEAM);
	} else {
		teleportNearHouseDoor(player, true);
	}
	if (ownerChanged)
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_CHANGE_OWNER());
	else
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_REQUEST_OUT());
}

void HouseController::teleportNearHouseDoor(model::gameobjects::player::Player& player, bool outsideHouse) {
	Ptr<SpawnTemplate> butler = getOwner().getButler()->getSpawn();
	Ptr<SpawnTemplate> relationshipCrystal = getOwner().getRelationshipCrystal()->getSpawn();
	float x, y, z; // midpoint between butler and relationship crystal, since we currently have no door coordinates in templates
	int8_t h = utils::PositionUtil::getHeadingTowards(*getOwner().getRelationshipCrystal(), butler->getX(), butler->getY());
	h = static_cast<int8_t>(h - 30); // this is the supposed heading towards the door (crystal is right from the door, so offset direction towards butler by 90 degrees)
	x = (butler->getX() + relationshipCrystal->getX()) / 2;
	y = (butler->getY() + relationshipCrystal->getY()) / 2;
	z = std::max(butler->getZ(), relationshipCrystal->getZ());
	if (outsideHouse) { // offset the midpoint 2.5m behind the butler, to get coords outside the house, near the door
		double radian = utils::PositionUtil::convertHeadingToAngle(h) * 0.017453292519943295; // Java: Math.toRadians
		x += static_cast<float>(std::cos(radian) * 2.5f);
		y += static_cast<float>(std::sin(radian) * 2.5f);
	} else {
		h = static_cast<int8_t>(h + (h < 60 ? 60 : -60)); // opposite direction (player should look inside house)
	}
	services::teleport::TeleportService::teleportTo(player, getOwner().getWorldId(), getOwner().getInstanceId(), x, y, z, h,
		TeleportAnimation::FADE_OUT_BEAM);
}

void HouseController::updateSign() {
	if (!getOwner().getCurrentSign())
		return;
	int32_t newNpcId = getCurrentSignNpcId();
	if (newNpcId != getOwner().getCurrentSign()->getNpcId()) {
		Ptr<SpawnTemplate> t = getOwner().getCurrentSign()->getSpawn();
		Ref<SpawnTemplate> newTemplate =
			spawnengine::SpawnEngine::newSingleTimeSpawn(t->getWorldId(), newNpcId, t->getX(), t->getY(), t->getZ(), t->getHeading(), t->getCreatorId());
		getOwner().updateSpawn(SpawnType::SIGN, runtime::cast<Npc>(spawnengine::SpawnEngine::spawnObject(*newTemplate, getOwner().getInstanceId())));
	}
}

void HouseController::updateHouseSpawns() {
	// only update spawns in active studios
	if (getOwner().getHouseType() == model::templates::housing::HouseType::STUDIO && (!getOwner().getPosition() || !getOwner().isSpawned()))
		return;
	getOwner().updateSpawn(SpawnType::MANAGER, nullptr); // remove old butler, otherwise new npcs spawn with old owner name
	updateSpawns();
	updateAppearance();
}

int32_t HouseController::getCurrentSignNpcId() {
	if (getOwner().getBids())
		return getOwner().getLand()->getSaleSignNpcId();
	if (getOwner().getOwnerId() == 0)
		return getOwner().getLand()->getNosaleSignNpcId(); // invisible npc
	return getOwner().isInactive() ? getOwner().getLand()->getWaitingSignNpcId() : getOwner().getLand()->getHomeSignNpcId();
}

} // namespace aion::gameserver::controllers
