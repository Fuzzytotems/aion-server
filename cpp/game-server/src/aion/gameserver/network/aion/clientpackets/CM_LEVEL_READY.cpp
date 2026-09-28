#include "aion/gameserver/network/aion/clientpackets/CM_LEVEL_READY.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WindstreamData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/animations/ArrivalAnimation.h"
#include "aion/gameserver/model/flypath/FlyPathType.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/windstreams/Location2D.h"
#include "aion/gameserver/model/templates/windstreams/StreamLocations.h"
#include "aion/gameserver/model/templates/windstreams/WindstreamTemplate.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACCOUNT_PROPERTIES.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CUBE_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_OBJECTS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_COUNT_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPGRADE_ARCADE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WINDSTREAM_ANNOUNCE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/TownService.h"
#include "aion/gameserver/services/WeatherService.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/rift/RiftInformer.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_LEVEL_READY::CM_LEVEL_READY(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

void CM_LEVEL_READY::readImpl() {
}

void CM_LEVEL_READY::runImpl() {
	using model::gameobjects::player::Player;
	runtime::Ptr<Player> activePlayer = getConnection()->getActivePlayer();
	if (activePlayer->getActiveHouse())
		sendPacket(serverpackets::SM_HOUSE_OBJECTS(activePlayer->getActiveHouse()->getRegistry()->getSpawnedObjects()));
	if (activePlayer->isInInstance()) {
		sendPacket(serverpackets::SM_INSTANCE_COUNT_INFO(activePlayer->getWorldId(), activePlayer->getInstanceId()));
	}
	sendPacket(serverpackets::SM_PLAYER_INFO(*activePlayer));
	activePlayer->getController().startProtectionActiveTask();
	sendPacket(serverpackets::SM_ACCOUNT_PROPERTIES());
	{
		std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::motion::Motion>> activeMotions;
		if (runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<model::gameobjects::player::motion::Motion>>> motions =
				activePlayer->getMotions().getActiveMotions()) {
			for (const auto& entry : motions->snapshot())
				activeMotions.emplace(entry.key, entry.value);
		}
		sendPacket(serverpackets::SM_MOTION(activePlayer->getObjectId(), activeMotions));
	}
	const model::templates::windstreams::WindstreamTemplate* windstream =
		dataholders::DataManager::WINDSTREAM_DATA->getStreamTemplate(activePlayer->getPosition()->getMapId());
	if (windstream != nullptr)
		for (const model::templates::windstreams::Location2D& location : windstream->getLocations()->getLocation()) {
			if (!location.getFlyPathType()) // Java: NullPointerException at getFlyPathType().getId()
				throw runtime::NullPointerException("Cannot invoke \"FlyPathType.getId()\" because the return value of \"Location2D.getFlyPathType()\" is null");
			// Java FlyPathType.getId(): GEYSER(0), ONE_WAY(1), TWO_WAY(2), the ordinal
			sendPacket(serverpackets::SM_WINDSTREAM_ANNOUNCE(static_cast<int32_t>(*location.getFlyPathType()), windstream->getMapId(), location.getId(),
				location.getState()));
		}
	// Spawn player into the world.
	world::World::getInstance().spawn(activePlayer);
	if (activePlayer->isInFlyState(model::gameobjects::state::FlyState::FLYING)) // notify client if we are still flying (client always ends flying after teleport)
		activePlayer->getFlyController().startFly(true, true);
	// SM_SHIELD_EFFECT, SM_ABYSS_ARTIFACT_INFO3
	if (activePlayer->isInSiegeWorld()) {
		services::SiegeService::getInstance().onEnterSiegeWorld(*activePlayer);
	}
	// SM_CONQUEROR_PROTECTOR
	services::conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().onEnterMap(*activePlayer);
	// SM_RIFT_ANNOUNCE
	services::rift::RiftInformer::sendRiftsInfo(*activePlayer);
	// SM_UPGRADE_ARCADE
	if (configs::main::EventsConfig::ENABLE_EVENT_ARCADE.load())
		sendPacket(serverpackets::SM_UPGRADE_ARCADE(true));
	// SM_NEARBY_QUESTS
	activePlayer->getController().updateNearbyQuests();
	// SM_QUEST_REPEAT
	activePlayer->getController().updateRepeatableQuests();
	// Loading weather for the player's region
	services::WeatherService::getInstance().loadWeather(*activePlayer);
	questEngine::QuestEngine::getInstance().onEnterWorld(*activePlayer);
	activePlayer->getController().onEnterWorld();
	services::instance::InstanceService::onEnterInstance(*activePlayer);
	activePlayer->getEffectController()->updatePlayerEffectIcons(nullptr);
	sendPacket(serverpackets::SM_CUBE_UPDATE::cubeSize(model::items::storage::StorageType::CUBE, *activePlayer));
	runtime::Ptr<model::gameobjects::Pet> pet = activePlayer->getPet();
	if (pet && !pet->isSpawned())
		world::World::getInstance().spawn(pet);
	activePlayer->setPortAnimation(model::animations::ArrivalAnimation::NONE);
	services::TownService::getInstance().onEnterWorld(*activePlayer);
	services::event::EventService::getInstance().onEnterMap(*activePlayer);
	runtime::Ptr<model::team::TemporaryPlayerTeam> team = activePlayer->getCurrentTeam();
	if (team)
		utils::ThreadPoolManager::getInstance().schedule(runtime::Pin(),
			[team = runtime::Ref<model::team::TemporaryPlayerTeam>(team), activePlayer = runtime::Ref<Player>(activePlayer)] { team->sendBrands(*activePlayer); },
			100); // delayed to fix brands when returning from studios/houses
}

AION_CLIENT_PACKET(CM_LEVEL_READY);

} // namespace aion::gameserver::network::aion::clientpackets
