#include "aion/gameserver/services/KiskService.h"

#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/animations/ActionAnimation.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACTION_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_KISK_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

KiskService::KiskService() = default;

KiskService::~KiskService() = default;

KiskService& KiskService::getInstance() {
	static KiskService instance; // Java SingletonHolder
	return instance;
}

// Java KiskService.java:28-52
void KiskService::removeKisk(model::gameobjects::Kisk& kisk) {
	// remove offline binds
	for (int32_t memberId : kisk.getCurrentMemberIds()) {
		boundButOfflinePlayer.remove(memberId);
	}

	for (int32_t obj : ownerPlayer.keySet()) {
		runtime::Ptr<model::gameobjects::Kisk> owned = ownerPlayer.get(obj);
		if (owned == nullptr) // Java: ownerPlayer.get(obj).equals(kisk) on a key removed meanwhile
			throw runtime::NullPointerException("ownerPlayer.get(" + std::to_string(obj) + ")");
		if (owned->equals(kisk)) {
			ownerPlayer.remove(obj);
			break;
		}
	}
	// remove the "2h place cooldown" for the kisk creator
	runtime::Ptr<model::gameobjects::player::Player> creator = world::World::getInstance().getPlayer(kisk.getCreatorId());
	if (creator != nullptr)
		utils::PacketSendUtility::sendPacket(*creator, network::aion::serverpackets::SM_KISK_UPDATE(kisk));

	for (const runtime::Ptr<model::gameobjects::player::Player>& member : kisk.getCurrentMemberList()) {
		teleport::TeleportService::sendKiskBindPoint(*member);
		member->setKisk(nullptr);
		if (member->isDead()) // player has died and is not revived
			member->getController().showResurrectionOptions();
	}
}

// Java KiskService.java:54-63
void KiskService::onBind(model::gameobjects::Kisk& kisk, model::gameobjects::player::Player& player) {
	if (runtime::Ptr<model::gameobjects::Kisk> current = player.getKisk())
		current->removePlayer(player);

	kisk.addPlayer(player);
	teleport::TeleportService::sendKiskBindPoint(player);
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_BINDSTONE_REGISTER());
	// Send Animated Bind Flash
	utils::PacketSendUtility::broadcastPacket(player,
		network::aion::serverpackets::SM_ACTION_ANIMATION(player.getObjectId(), model::animations::ActionAnimation::BIND_KISK), true);
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
