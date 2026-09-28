#include "aion/gameserver/skillengine/condition/PolishChargeCondition.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine::condition {

using gameserver::model::gameobjects::Item;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::items::ItemSlot;
using runtime::Ptr;

bool PolishChargeCondition::validate(model::Skill& env) const {
	if (Ptr<Player> effector = runtime::as<Player>(env.getEffector())) {
		for (Ptr<Item> item : effector->getEquipment().getEquippedItems()) {
			if (item->getItemTemplate()->isWeapon() && item->getIdianStone()) {
				if ((item->getEquipmentSlot() & gameserver::model::items::getSlotIdMask(ItemSlot::MAIN_OFF_HAND)) != 0
					|| (item->getEquipmentSlot() & gameserver::model::items::getSlotIdMask(ItemSlot::SUB_OFF_HAND)) != 0) {
					continue;
				}
				item->getIdianStone()->decreasePolishCharge(*effector, value);
			}
		}
	}
	return true;
}

} // namespace aion::gameserver::skillengine::condition
