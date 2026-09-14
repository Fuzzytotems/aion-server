#include "aion/gameserver/services/ShieldService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/siege/SiegeShield.h"
#include "aion/gameserver/model/templates/shield/ShieldTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.ShieldService");

ShieldService::ShieldService() {
	AION_UNPORTED();
}

void ShieldService::logDetachedShields() {
	AION_UNPORTED();
}

runtime::Ref<controllers::observer::ShieldObserver> ShieldService::createShieldObserver(model::siege::FortressLocation& location,
	model::gameobjects::Creature& observed) {
	AION_UNPORTED();
}

runtime::Ptr<model::siege::SiegeShield> ShieldService::tryRegisterShield(int32_t worldId, geoEngine::scene::Spatial& geometry) {
	AION_UNPORTED();
}

void ShieldService::attachShield(model::siege::SiegeLocation& location) {
	AION_UNPORTED();
}

bool ShieldService::isShieldInsideLocation(model::siege::SiegeShield& shield, model::siege::SiegeLocation& location) {
	AION_UNPORTED();
}

bool ShieldService::isIgnored(int32_t mapId, std::string_view geometryName) {
	AION_UNPORTED();
}

ShieldService& ShieldService::getInstance() {
	static ShieldService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
