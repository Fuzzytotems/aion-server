#include "aion/gameserver/model/templates/item/actions/CompositionAction.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

namespace Rnd = commons::utils::Rnd;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

// Java CompositionAction.java:18-35
bool CompositionAction::canAct(gameobjects::player::Player& /*player*/, gameobjects::Item& tools, gameobjects::Item& first, gameobjects::Item& second) const {

	if (!tools.getItemTemplate()->isCombinationItem())
		return false;

	if (!first.getItemTemplate()->isEnchantmentStone())
		return false;

	if (!second.getItemTemplate()->isEnchantmentStone())
		return false;

	if (first.getItemCount() < 1 || second.getItemCount() < 1)
		return false;

	return first.getItemTemplate()->getLevel() <= 95 && second.getItemTemplate()->getLevel() <= 95;
}

// Java CompositionAction.java:37-45
void CompositionAction::act(gameobjects::player::Player& player, gameobjects::Item& tools, gameobjects::Item& first, gameobjects::Item& second) const {
	bool result = player.getInventory().decreaseByItemId(tools.getItemId(), 1);
	bool result1 = player.getInventory().decreaseByItemId(first.getItemId(), 1);
	bool result2 = player.getInventory().decreaseByItemId(second.getItemId(), 1);
	if (result && result1 && result2) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMPOUND_SUCCESS(second.getL10n(), first.getL10n()));
		services::item::ItemService::addItem(player, getItemId(calcLevel(first.getItemTemplate()->getLevel(), second.getItemTemplate()->getLevel())), 1);
	}
}

// Java CompositionAction.java:47-57
int32_t CompositionAction::calcLevel(int32_t first, int32_t second) const {
	int32_t value = ((first + second) / 2);
	if (value < 11) {
		value = Rnd::get(1, 20);
	} else {
		int32_t random = Rnd::get(1, 10);
		int32_t bit = Rnd::get(0, 1);
		value = (bit == 0 ? value - random : value + random);
	}
	return value;
}

// Java CompositionAction.java:59-61
int32_t CompositionAction::getItemId(int32_t value) const {
	return 166000000 + value;
}

} // namespace aion::gameserver::model::templates::item::actions
