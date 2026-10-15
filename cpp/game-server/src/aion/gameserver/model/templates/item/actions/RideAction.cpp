#include "aion/gameserver/model/templates/item/actions/RideAction.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/RideData.h"
#include "aion/gameserver/model/ActionStateInfo.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/ride/RideInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using controllers::observer::ActionObserver;
using controllers::observer::ObserverType;
using gameobjects::Creature;
using gameobjects::Item;
using gameobjects::player::Player;
using gameobjects::state::CreatureState;
using model::actions::PlayerMode;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ref;
using skillengine::effect::AbnormalState;
using utils::PacketSendUtility;

/** Java: the anonymous ItemUseObserver of act (RideAction.java:70-79, fieldmap key RideAction$1) */
struct RideAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;
	const Ref<Item> parentItem;

	static Ref<RideAction_ItemUseObserver> create(Player& player, Item& parentItem) {
		return runtime::makeRef<RideAction_ItemUseObserver>(player, parentItem);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED());
		PacketSendUtility::broadcastPacket(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), parentItem->getObjectId(), parentItem->getItemId(), 0, 3, 0),
			true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	RideAction_ItemUseObserver(Player& playerValue, Item& parentItemValue) : player(Ref<Player>(playerValue)), parentItem(Ref<Item>(parentItemValue)) {}
	~RideAction_ItemUseObserver() override = default;
};

/** Java: the anonymous ActionObserver(ABNORMALSETTED) of finishUse (RideAction.java:105-114, fieldmap key RideAction$2) */
struct RideAction_ActionObserver final : ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;

	static Ref<RideAction_ActionObserver> create(Player& player) { return runtime::makeRef<RideAction_ActionObserver>(player); }

	void abnormalsetted(AbnormalState state) override {
		if ((controllers::detail::getAbnormalStateId(state) & controllers::detail::getAbnormalStateId(AbnormalState::DISMOUNT_RIDE)) != 0) {
			player->unsetPlayerMode(PlayerMode::RIDE);
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_UNRIDE_ABNORMAL_STATE());
		}
	}

protected:
	explicit RideAction_ActionObserver(Player& playerValue) : ActionObserver(ObserverType::ABNORMALSETTED), player(Ref<Player>(playerValue)) {}
	~RideAction_ActionObserver() override = default;
};

/** Java: the anonymous ActionObserver(ATTACKED) of finishUse (RideAction.java:119-126, fieldmap key RideAction$3) */
struct RideAction_ActionObserver_2 final : ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;

	static Ref<RideAction_ActionObserver_2> create(Player& player) { return runtime::makeRef<RideAction_ActionObserver_2>(player); }

	void attacked(Creature& /*creature*/, int32_t /*skillId*/) override {
		if (commons::utils::Rnd::chance() < 20) // 20% from client action file
			player->unsetPlayerMode(PlayerMode::RIDE);
	}

protected:
	explicit RideAction_ActionObserver_2(Player& playerValue) : ActionObserver(ObserverType::ATTACKED), player(Ref<Player>(playerValue)) {}
	~RideAction_ActionObserver_2() override = default;
};

/** Java: the anonymous ActionObserver(DOT_ATTACKED) of finishUse (RideAction.java:130-137, fieldmap key RideAction$4) */
struct RideAction_ActionObserver_3 final : ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player;

	static Ref<RideAction_ActionObserver_3> create(Player& player) { return runtime::makeRef<RideAction_ActionObserver_3>(player); }

	void dotattacked(Creature& /*creature*/, skillengine::model::Effect& /*dotEffect*/) override {
		if (commons::utils::Rnd::chance() < 20) // 20% from client action file
			player->unsetPlayerMode(PlayerMode::RIDE);
	}

protected:
	explicit RideAction_ActionObserver_3(Player& playerValue) : ActionObserver(ObserverType::DOT_ATTACKED), player(Ref<Player>(playerValue)) {}
	~RideAction_ActionObserver_3() override = default;
};

} // namespace

// Java RideAction.java:44-67
bool RideAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItem, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
	if (!player.isInPlayerMode(PlayerMode::RIDE)) { // RideAction is for mounting and dismounting, canAct should never forbid dismounting
		if (parentItem == nullptr)
			return false;

		if (configs::main::CustomConfig::ENABLE_RIDE_RESTRICTION) {
			for (const runtime::Ptr<world::zone::ZoneInstance>& zone : player.findZones()) {
				if (!zone->canRide()) {
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_RIDE_INVALID_LOCATION());
					return false;
				}
			}
		}
		if (player.isInState(CreatureState::RESTING)) {
			// Java ActionState.RESTING.getL10n(): the L10n default method spelled out at the call site (ActionStateInfo.h)
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_RIDE(utils::ChatUtil::l10n(getL10nId(ActionState::RESTING))));
			return false;
		}
		if (player.getEffectController()->isInAnyAbnormalState(AbnormalState::DISMOUNT_RIDE)) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_RIDE_ABNORMAL_STATE());
			return false;
		}
	}
	return true;
}

// Java RideAction.java:69-93
void RideAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> parentItemPtr, runtime::Ptr<gameobjects::Item> /*targetItem*/,
	std::initializer_list<std::any> /*params*/) const {
	if (player.isInPlayerMode(PlayerMode::RIDE)) {
		player.unsetPlayerMode(PlayerMode::RIDE);
		return;
	}
	Item& parentItem = *parentItemPtr;
	int32_t castingDelay = parentItem.getItemTemplate()->getCastingDelay();
	if (castingDelay <= 0) {
		finishUse(player, parentItem);
	} else {
		PacketSendUtility::broadcastPacket(player,
			SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), castingDelay, 0, 0), true);
		Ref<RideAction_ItemUseObserver> observer = RideAction_ItemUseObserver::create(player, parentItem);
		player.getObserveController()->attach(*observer);
		// Java lambda RideAction.java:89-92: pins this (static data), the observer, the player and the item
		RideAction_ItemUseObserver& itemUseObserver = *observer;
		player.getController().addTask(TaskId::ITEM_USE,
			utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &parentItem},
				[this, &player, &itemUseObserver, &parentItem] {
					player.getObserveController()->removeObserver(itemUseObserver);
					finishUse(player, parentItem);
				},
				castingDelay));
	}
}

// Java RideAction.java:95-145
void RideAction::finishUse(gameobjects::player::Player& player, gameobjects::Item& parentItem) const {
	if (!canAct(player, runtime::Ptr<Item>(&parentItem), nullptr)) {
		PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 3, 0),
			true);
		return;
	}
	player.startCooldown(parentItem);
	PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(parentItem.getL10n()));
	player.unsetState(CreatureState::ACTIVE);
	player.setState(CreatureState::RESTING);
	if (player.isInFlyingState())
		player.setState(CreatureState::FLOATING_CORPSE);
	const ItemTemplate* itemTemplate = parentItem.getItemTemplate();
	player.setPlayerMode(PlayerMode::RIDE, std::any(getRideInfo()));

	Ref<RideAction_ActionObserver> rideObserver = RideAction_ActionObserver::create(player);
	player.getObserveController()->addObserver(*rideObserver);
	player.addRideObserver(*rideObserver);

	// TODO some mounts have lower chance of dismounting
	Ref<RideAction_ActionObserver_2> attackedObserver = RideAction_ActionObserver_2::create(player);
	player.getObserveController()->addObserver(*attackedObserver);
	player.addRideObserver(*attackedObserver);

	Ref<RideAction_ActionObserver_3> dotAttackedObserver = RideAction_ActionObserver_3::create(player);
	player.getObserveController()->addObserver(*dotAttackedObserver);
	player.addRideObserver(*dotAttackedObserver);

	PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::CHANGE_SPEED, 0, 0), true);
	const ride::RideInfo* rideInfo = getRideInfo();
	if (rideInfo == nullptr) // Java: getRideInfo().getNpcId() on null
		throw runtime::NullPointerException("RideAction.getRideInfo()");
	PacketSendUtility::broadcastPacket(player, SM_EMOTION(player, EmotionType::RIDE, 0, rideInfo->getNpcId()), true);
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), parentItem.getObjectId(), parentItem.getItemId(), 0, 1, 1), true);
	questEngine::QuestEngine::getInstance().rideAction(*questEngine::model::QuestEnv::create(nullptr, player, 0), itemTemplate->getTemplateId());
}

// Java RideAction.java:147-149
const ride::RideInfo* RideAction::getRideInfo() const {
	return dataholders::DataManager::RIDE_DATA->getRideInfo(npcId);
}

} // namespace aion::gameserver::model::templates::item::actions
