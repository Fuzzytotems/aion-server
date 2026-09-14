#include "aion/gameserver/services/KiskService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"

namespace aion::gameserver::services {

KiskService::KiskService() = default;

KiskService::~KiskService() = default;

KiskService& KiskService::getInstance() {
	static KiskService instance; // Java SingletonHolder
	return instance;
}

void KiskService::removeKisk(model::gameobjects::Kisk& kisk) {
	AION_UNPORTED();
}

void KiskService::onBind(model::gameobjects::Kisk& kisk, model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void KiskService::onLogin(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void KiskService::onLogout(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void KiskService::regKisk(model::gameobjects::Kisk& kisk, std::optional<int32_t> objOwnerId) {
	AION_UNPORTED();
}

bool KiskService::haveKisk(std::optional<int32_t> objOwnerId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
