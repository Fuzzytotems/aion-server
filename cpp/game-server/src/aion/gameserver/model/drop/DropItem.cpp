#include "aion/gameserver/model/drop/DropItem.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/drop/Drop.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::drop {

DropItem::DropItem(const Drop* value)
	: dropTemplate(value), optionalSocket() {
	// Java: this.playerObjIds = new ArrayList<>(); if (DataManager.ITEM_DATA.getItemTemplate(dropTemplate.getItemId()).getOptionSlotBonus() != 0)
	// optionalSocket = -1
	AION_UNPORTED();
}

runtime::Ref<DropItem> DropItem::create(const Drop* value) {
	return runtime::makeRef<DropItem>(value);
}

void DropItem::calculateCount() {
	AION_UNPORTED();
}

bool DropItem::canViewDropItem(int32_t objId) {
	AION_UNPORTED();
}

void DropItem::setPlayerObjId(int32_t playerObjId) {
	AION_UNPORTED();
}

void DropItem::setWinningPlayer(runtime::Ptr<gameobjects::player::Player> value) {
	this->winningPlayer.set(value);
}

runtime::Ptr<gameobjects::player::Player> DropItem::getWinningPlayer() {
	AION_UNPORTED();
}

bool DropItem::isOnlyPossibleLooter(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

int32_t DropItem::getLootEffectId() {
	AION_UNPORTED();
}

DropItem::~DropItem() = default;

} // namespace aion::gameserver::model::drop
