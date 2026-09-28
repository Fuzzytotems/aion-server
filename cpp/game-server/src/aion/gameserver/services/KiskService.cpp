#include "aion/gameserver/services/KiskService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

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
	runtime::Ptr<model::gameobjects::Kisk> kisk = this->boundButOfflinePlayer.get(player.getObjectId());
	if (kisk) {
		kisk->addPlayer(player);
		this->boundButOfflinePlayer.remove(player.getObjectId());
	}
}

void KiskService::onLogout(model::gameobjects::player::Player& player) {
	runtime::Ptr<model::gameobjects::Kisk> kisk = player.getKisk();
	// store binding if existent
	if (kisk) {
		this->boundButOfflinePlayer.put(player.getObjectId(), runtime::Ref<model::gameobjects::Kisk>(kisk));
	}
}

void KiskService::regKisk(model::gameobjects::Kisk& kisk, std::optional<int32_t> objOwnerId) {
	if (!objOwnerId)
		throw runtime::NullPointerException("objOwnerId"); // Java: ConcurrentHashMap.put(null, ...)
	ownerPlayer.put(*objOwnerId, runtime::Ref<model::gameobjects::Kisk>(kisk));
}

bool KiskService::haveKisk(std::optional<int32_t> objOwnerId) {
	if (!objOwnerId)
		throw runtime::NullPointerException("objOwnerId"); // Java: ConcurrentHashMap.containsKey(null)
	return ownerPlayer.containsKey(*objOwnerId);
}

} // namespace aion::gameserver::services
