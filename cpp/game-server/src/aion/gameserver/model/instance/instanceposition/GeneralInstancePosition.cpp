#include "aion/gameserver/model/instance/instanceposition/GeneralInstancePosition.h"

#include "aion/gameserver/services/teleport/TeleportService.h"

namespace aion::gameserver::model::instance::instanceposition {

GeneralInstancePosition::GeneralInstancePosition() = default;

GeneralInstancePosition::~GeneralInstancePosition() = default;

// Java GeneralInstancePosition.java:14-18
void GeneralInstancePosition::initialize(int32_t mapId_, int32_t instanceId_) {
	this->mapId.set(mapId_);
	this->instanceId.set(instanceId_);
}

// Java GeneralInstancePosition.java:20-22
void GeneralInstancePosition::teleport(gameobjects::player::Player& player, float x, float y, float z, int8_t h) {
	services::teleport::TeleportService::teleportTo(player, mapId.get(), instanceId.get(), x, y, z, h);
}

} // namespace aion::gameserver::model::instance::instanceposition
