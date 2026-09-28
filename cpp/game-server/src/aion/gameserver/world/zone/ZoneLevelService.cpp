#include "aion/gameserver/world/zone/ZoneLevelService.h"

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"

namespace aion::gameserver::world::zone {

void ZoneLevelService::checkZoneLevels(model::gameobjects::player::Player& player) {
	World& world = World::getInstance();
	float z = player.getZ();

	if (player.isDead())
		return;

	if (z < static_cast<float>(world.getWorldMap(player.getWorldId())->getDeathLevel())) {
		player.getController().die();
		return;
	}

	float noseHeight = player.getPlayerAppearance()->getBoundHeight() - 0.1f;
	if (z + noseHeight < static_cast<float>(world.getWorldMap(player.getWorldId())->getWaterLevel()))
		startDrowning(player);
	else
		stopDrowning(player);
}

void ZoneLevelService::stopDrowning(model::gameobjects::player::Player& player) {
	if (player.getController().hasTask(model::TaskId::DROWN))
		player.getController().cancelTask(model::TaskId::DROWN);
}

void ZoneLevelService::startDrowning(model::gameobjects::player::Player& player) {
	if (player.getController().hasTask(model::TaskId::DROWN))
		return;
	// lambda at ZoneLevelService.java:47: pin {&player}
	player.getController().addTask(model::TaskId::DROWN, utils::ThreadPoolManager::getInstance().scheduleAtFixedRate({&player}, [&player] {
		int32_t value = player.getLifeStats()->getMaxHp() / 20;
		if (player.getLifeStats()->reduceHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::DROWNING, value, 0,
				network::aion::serverpackets::SM_ATTACK_STATUS_LOG::REGULAR, player) == 0)
			stopDrowning(player);
	}, 0, DROWN_PERIOD));
}

} // namespace aion::gameserver::world::zone
