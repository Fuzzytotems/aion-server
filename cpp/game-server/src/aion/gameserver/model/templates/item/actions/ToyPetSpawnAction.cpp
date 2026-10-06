#include "aion/gameserver/model/templates/item/actions/ToyPetSpawnAction.h"

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/KiskService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/spawnengine/VisibleObjectSpawner.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::Item;
using gameobjects::Kisk;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ref;
using services::KiskService;
using utils::PacketSendUtility;

/** Java: the anonymous ItemUseObserver of act (ToyPetSpawnAction.java:76-84, fieldmap key ToyPetSpawnAction$1); unlike the other item
 * observers it does not remove itself */
struct ToyPetSpawnAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;
	const Ref<Item> parentItem;

	static Ref<ToyPetSpawnAction_ItemUseObserver> create(Player& player, Item& parentItem) {
		return runtime::makeRef<ToyPetSpawnAction_ItemUseObserver>(player, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED());
		PacketSendUtility::broadcastPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 2, 0), true);
	}

protected:
	ToyPetSpawnAction_ItemUseObserver(Player& playerValue, Item& parentItemValue) : player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~ToyPetSpawnAction_ItemUseObserver() override = default;
};

/** Java private isPutKiskZone(Player) (ToyPetSpawnAction.java:129-135): no instance state, a file-local helper */
bool isPutKiskZone(Player& player) {
	for (const runtime::Ptr<world::zone::ZoneInstance>& zone : player.findZones()) {
		if (!zone->canPutKisk())
			return false;
	}
	return true;
}

} // namespace

// Java ToyPetSpawnAction.java:45-64
bool ToyPetSpawnAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	if (player.isFlying()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_BINDSTONE_ITEM_WHILE_FLYING());
		return false;
	}
	if (player.isInInstance()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_REGISTER_BINDSTONE_FAR_FROM_NPC());
		return false;
	}
	if (KiskService::getInstance().haveKisk(player.getObjectId()) && configs::main::CustomConfig::ENABLE_KISK_RESTRICTION) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_BINDSTONE_ALREADY_INSTALLED());
		return false;
	}
	if (!isPutKiskZone(player)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_LOCATION());
		return false;
	}
	return true;
}

// Java ToyPetSpawnAction.java:66-91
void ToyPetSpawnAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	Item& parentItem = *parentItemPtr;
	// ShowAction
	int32_t castingDelay = parentItem.getItemTemplate()->getCastingDelay();
	if (castingDelay <= 0) {
		finishUse(player, parentItem);
		return;
	}
	PacketSendUtility::broadcastPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), castingDelay, 0, 0), true);
	Ref<ToyPetSpawnAction_ItemUseObserver> observer = ToyPetSpawnAction_ItemUseObserver::create(player, parentItem);

	player.getObserveController()->attach(*observer);
	// Java lambda ToyPetSpawnAction.java:87-90: pins this (static data), the observer, the player and the item
	ToyPetSpawnAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parentItem},
			[this, &player, &itemUseObserver, &parentItem] {
				player.getObserveController()->removeObserver(itemUseObserver);
				finishUse(player, parentItem);
			},
			castingDelay));
}

// Java ToyPetSpawnAction.java:93-127
void ToyPetSpawnAction::finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem) const {
	if (!canAct(player, runtime::Ptr<Item>(&parentItem), nullptr)) {
		PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 2, 0),
			true);
		return;
	}
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem.getL10n()));
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 1, 1), true);
	// RemoveKisk
	if (!player.getInventory().decreaseByObjectId(parentItem.getObjectId(), 1))
		return;
	player.startCooldown(parentItem);
	float x = player.getX();
	float y = player.getY();
	float z = player.getZ();
	int8_t heading = static_cast<int8_t>((player.getHeading() + 60) % 120);
	int32_t worldId = player.getWorldId();
	int32_t instanceId = player.getInstanceId();
	Ref<spawns::SpawnTemplate> spawn = spawnengine::SpawnEngine::newSingleTimeSpawn(worldId, npcid, x, y, z, heading);

	const Ref<Kisk> kiskRef = spawnengine::VisibleObjectSpawner::spawnKisk(*spawn, instanceId, player);
	Kisk& kisk = *kiskRef;
	const int32_t objOwnerId = player.getObjectId();
	// Schedule Despawn Action (Java lambda ToyPetSpawnAction.java:115: pins the kisk; stored as its controller's DESPAWN task)
	runtime::FutureRef task = utils::ThreadPoolManager::getInstance().schedule({&kisk}, [&kisk] { kisk.getController().delete_(); },
		kisk.getRemainingLifetime(), runtime::TimeUnit::SECONDS);
	kisk.getController().addTask(TaskId::DESPAWN, std::move(task));

	// ShowFinalAction
	KiskService::getInstance().regKisk(kisk, objOwnerId);

	if (kisk.getMaxMembers() > 1)
		kisk.getController().onDialogRequest(player);
	else
		KiskService::getInstance().onBind(kisk, player);
}

} // namespace aion::gameserver::model::templates::item::actions
