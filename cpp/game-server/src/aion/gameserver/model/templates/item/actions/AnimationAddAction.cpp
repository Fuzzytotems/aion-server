#include "aion/gameserver/model/templates/item/actions/AnimationAddAction.h"

#include <cstdint>
#include <unordered_map>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::Item;
using gameobjects::player::Player;
using gameobjects::player::motion::Motion;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_MOTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ref;
using utils::PacketSendUtility;

/** Java: the anonymous ItemUseObserver of act (AnimationAddAction.java:57-66, fieldmap key AnimationAddAction$1) */
struct AnimationAddAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;     // captured param final Player player
	const Ref<Item> parentItem;   // captured param final Item parentItem

	static Ref<AnimationAddAction_ItemUseObserver> create(Player& player, Item& parentItem) {
		return runtime::makeRef<AnimationAddAction_ItemUseObserver>(player, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED());
		PacketSendUtility::sendPacket(*player,
			SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemTemplate()->getTemplateId(), 0, 3, 0));
		player->getObserveController()->removeObserver(*this);
	}

protected:
	AnimationAddAction_ItemUseObserver(Player& playerValue, Item& parentItemValue) : player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~AnimationAddAction_ItemUseObserver() override = default;
};

/** Java: (int) (System.currentTimeMillis() / 1000) + minutes * 60, int arithmetic (wraps like Java) */
int32_t expireTimeOf(std::optional<int32_t> minutes) {
	if (!minutes)
		return 0;
	return static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000)) +
		static_cast<uint32_t>(*minutes) * 60u);
}

/** Java private AnimationAddAction.addMotion(Player, int) (AnimationAddAction.java:99-103); a file-local helper (no header request) */
void addMotion(Player& player, int32_t motionId, std::optional<int32_t> minutes) {
	Ref<Motion> motion = Motion::create(motionId, expireTimeOf(minutes), true);
	player.getMotions().add(*motion, true);
	PacketSendUtility::sendPacket(player, SM_MOTION(static_cast<int16_t>(motion->getId()), motion->secondsUntilExpiration()));
}

} // namespace

// Java private AnimationAddAction.finishUse(Player, Item) (AnimationAddAction.java:74-97); file-local in Java order through a member helper
void AnimationAddAction::finishUse(Player& player, Item& parentItem) const {
	if (player.getInventory().decreaseItemCount(parentItem, 1) != 0)
		return;
	player.startCooldown(parentItem);
	if (idle) {
		addMotion(player, *idle, minutes);
	}
	if (run) {
		addMotion(player, *run, minutes);
	}
	if (jump) {
		addMotion(player, *jump, minutes);
	}
	if (rest) {
		addMotion(player, *rest, minutes);
	}
	if (shop) {
		addMotion(player, *shop, minutes);
	}
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem.getL10n()));
	PacketSendUtility::broadcastPacketAndReceive(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 1, 0));
	// Java: new SM_MOTION(player.getObjectId(), player.getMotions().getActiveMotions()); the motions above made the map
	std::unordered_map<int32_t, runtime::Ptr<Motion>> activeMotions;
	if (runtime::Ptr<runtime::RcLinkedHashMap<int32_t, Ref<Motion>>> motions = player.getMotions().getActiveMotions()) {
		for (const auto& entry : motions->entrySet())
			activeMotions.emplace(entry.getKey(), entry.getValue());
	}
	PacketSendUtility::broadcastPacket(player, SM_MOTION(player.getObjectId(), activeMotions), false);
}

// Java AnimationAddAction.java:37-44
bool AnimationAddAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	if (parentItem == nullptr) { // no item selected.
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_COLOR_ERROR());
		return false;
	}
	return true;
}

// Java AnimationAddAction.java:46-72
void AnimationAddAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	Item& parentItem = *parentItemPtr;
	int32_t castingDelay = parentItem.getItemTemplate()->getCastingDelay();
	if (castingDelay <= 0) {
		finishUse(player, parentItem);
		return;
	}
	Ref<AnimationAddAction_ItemUseObserver> observer = AnimationAddAction_ItemUseObserver::create(player, parentItem);
	player.getObserveController()->attach(*observer);
	PacketSendUtility::sendPacket(player,
		SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemTemplate()->getTemplateId(), castingDelay, 0, 0));
	// Java lambda AnimationAddAction.java:69-72: pins this (static data), the observer, the player and the item
	AnimationAddAction_ItemUseObserver& itemUseObserver = *observer;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parentItem},
			[this, &player, &itemUseObserver, &parentItem] {
				player.getObserveController()->removeObserver(itemUseObserver);
				finishUse(player, parentItem);
			},
			castingDelay));
}

} // namespace aion::gameserver::model::templates::item::actions
