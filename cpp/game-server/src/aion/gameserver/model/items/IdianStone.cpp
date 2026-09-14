#include "aion/gameserver/model/items/IdianStone.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/RandomBonusEffect.h"

namespace aion::gameserver::model::items {

IdianStone::IdianStone(int32_t itemIdValue, PersistentState persistentStateValue, gameobjects::Item& itemValue, int32_t polishNumber,
	int32_t polishChargeValue)
	: OwnedPart(itemValue), ItemStone(itemValue.getObjectId(), itemIdValue, 0, persistentStateValue), polishCharge(polishChargeValue),
	  item(itemValue), burnDefend(0), burnAttack(0), rndBonusEffect() {
	// Java: burnDefend/burnAttack from item.getItemTemplate().getIdianAction(), rndBonusEffect = new RandomBonusEffect(StatBonusType.POLISH,
	// DataManager.ITEM_DATA.getItemTemplate(itemId).getActions().getPolishAction().getPolishSetId(), polishNumber)
	static_cast<void>(polishNumber);
	AION_UNPORTED();
}

IdianStone::~IdianStone() = default;

// Callback struct (fieldmap.py --class): IdianStone$1 (ActionObserver), stored in actionListener
void IdianStone::onEquip(gameobjects::player::Player& player, int64_t slotValue) {
	AION_UNPORTED();
}

void IdianStone::decreasePolishCharge(gameobjects::player::Player& player, bool isAttacked) {
	AION_UNPORTED();
}

void IdianStone::decreasePolishCharge(gameobjects::player::Player& player, int32_t skillValue) {
	AION_UNPORTED();
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED(*this)
void IdianStone::decreasePolishCharge(gameobjects::player::Player& player, bool isAttacked, int32_t skillValue) {
	AION_UNPORTED();
}

int32_t IdianStone::getPolishNumber() {
	AION_UNPORTED();
}

void IdianStone::onUnEquip(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void IdianStone::breakActionListener() noexcept {
	actionListener.set(nullptr);
}

} // namespace aion::gameserver::model::items
