#include "aion/gameserver/model/templates/item/actions/MultiReturnAction.h"

#include <algorithm>
#include <any>
#include <cctype>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/observer/ItemUseObserver.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MultiReturnItemData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ReturnLocList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::model::templates::item::actions {

namespace {

using gameobjects::Item;
using gameobjects::player::Player;
using network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ref;
using utils::PacketSendUtility;

/**
 * Java: the anonymous ItemUseObserver of act (MultiReturnAction.java:46-56, fieldmap key MultiReturnAction$1), stored in the player's
 * ObserveController until the task or abort() removes it.
 */
struct MultiReturnAction_ItemUseObserver final : controllers::observer::ItemUseObserver {
	AION_MAKE_REF_FRIEND

	const Ref<Player> player; // captured param Player player
	const Ref<Item> item;     // captured param Item item

	static Ref<MultiReturnAction_ItemUseObserver> create(Player& player, Item& item) {
		return runtime::makeRef<MultiReturnAction_ItemUseObserver>(player, item);
	}

	void abort() override {
		player->getController().cancelTask(TaskId::ITEM_USE);
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_ITEM_CANCELED());
		PacketSendUtility::broadcastPacket(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), item->getObjectId(), item->getItemId(), 0, 2, 0), true);
		player->getObserveController()->removeObserver(*this);
	}

protected:
	MultiReturnAction_ItemUseObserver(Player& playerValue, Item& itemValue) : player(Ref<Player>(playerValue)), item(Ref<Item>(itemValue)) {}
	~MultiReturnAction_ItemUseObserver() override = default;
};

/** Java String.toUpperCase() of the alias (the aliases are ASCII, portal_template2.xml) */
std::string toUpperCase(std::string text) {
	std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return text;
}

/**
 * Java private MultiReturnAction.finishUse(Player, Item, ItemUseObserver, int) (MultiReturnAction.java:69-81). C++: a file-local function that
 * takes the action's id (the header declares no finishUse; a file-local helper, as ReadAction's).
 */
void finishUse(int32_t id, Player& player, Item& item, MultiReturnAction_ItemUseObserver& observer, int32_t indexReturn) {
	// Java: DataManager.MULTIRETURN_DATA.getReturnLocListById(id).get(indexReturn) - a NullPointerException for an id without a list and an
	// IndexOutOfBoundsException for an index the client sent outside it, both kept
	const std::vector<ReturnLocList>* list = dataholders::DataManager::MULTIRETURN_DATA->getReturnLocListById(id);
	if (list == nullptr)
		throw runtime::NullPointerException("MultiReturnItemData.getReturnLocListById(" + std::to_string(id) + ")");
	if (indexReturn < 0 || static_cast<size_t>(indexReturn) >= list->size())
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(indexReturn) + " out of bounds for length " + std::to_string(list->size()));
	const ReturnLocList& loc = (*list)[static_cast<size_t>(indexReturn)];
	// Java: loc != null && loc.getAlias() != null && loc.getWorldid() > 0 - a JAXB element is never null, an absent alias is the empty string
	if (!loc.getAlias().empty() && loc.getWorldid() > 0) {
		if (!player.getInventory().decreaseByObjectId(item.getObjectId(), 1)) {
			observer.abort();
			return;
		}
		player.startCooldown(item);
		services::teleport::TeleportService::useTeleportScroll(player, toUpperCase(loc.getAlias()), loc.getWorldid());
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_USE_ITEM(item.getL10n()));
	}
}

} // namespace

// Java MultiReturnAction.java:30-33
bool MultiReturnAction::canAct(gameobjects::player::Player& /*player*/, runtime::Ptr<gameobjects::Item> /*item*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	return true;
}

// Java MultiReturnAction.java:35-67
void MultiReturnAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> itemPtr,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> params) const {
	Item& item = *itemPtr; // Java dereferences item first; CM_USE_ITEM passes the used item, never null
	int32_t castingDelay = item.getItemTemplate()->getCastingDelay();
	// Java: (int) params[0] - CM_USE_ITEM passes the int indexReturn (CM_USE_ITEM.java:104, :120)
	int32_t indexReturn = std::any_cast<int32_t>(params.begin()[0]);
	PacketSendUtility::broadcastPacket(player, SM_ITEM_USAGE_ANIMATION(player.getObjectId(), item.getObjectId(), item.getItemId(), castingDelay, 0, 0), true);

	Ref<MultiReturnAction_ItemUseObserver> observer = MultiReturnAction_ItemUseObserver::create(player, item);
	if (castingDelay <= 0) {
		finishUse(id, player, item, *observer, indexReturn);
		return;
	}

	player.getObserveController()->attach(*observer);
	// Java lambda MultiReturnAction.java:62-65: pins this (static data), the observer, the player and the item
	MultiReturnAction_ItemUseObserver& itemUseObserver = *observer;
	const int32_t actionId = id;
	player.getController().addTask(TaskId::ITEM_USE,
		utils::ThreadPoolManager::getInstance().schedule({this, &player, &itemUseObserver, &item},
			[actionId, &player, &itemUseObserver, &item, indexReturn] {
				player.getObserveController()->removeObserver(itemUseObserver);
				finishUse(actionId, player, item, itemUseObserver, indexReturn);
			},
			castingDelay));
}

} // namespace aion::gameserver::model::templates::item::actions
