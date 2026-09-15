#include "aion/gameserver/model/items/ChargeInfo.h"

#include "aion/gameserver/runtime/base/Checked.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::model::items {

ChargeInfo::ChargeInfo(int32_t chargePointsValue, gameobjects::Item& itemValue)
	: ActionObserver(controllers::observer::ObserverType::DOT_ATTACK_DEFEND), attackBurn(0), defendBurn(0), item(itemValue),
	  chargePoints(chargePointsValue) {
	// Java: attackBurn/defendBurn from item.getImprovement() (template), 0 without one
	AION_UNPORTED();
}

ChargeInfo::~ChargeInfo() = default;

runtime::Ref<ChargeInfo> ChargeInfo::create(int32_t chargePointsValue, gameobjects::Item& itemValue) {
	return runtime::makeRef<ChargeInfo>(chargePointsValue, itemValue);
}

gameobjects::Item& ChargeInfo::getItem() const {
	AION_CHECK("C4", item.isManaged(), "ChargeInfo.item: the Item was destroyed while its ChargeInfo is still referenced (ChargeInfo.h)");
	return item;
}

runtime::Ptr<gameobjects::player::Player> ChargeInfo::getPlayer() {
	AION_UNPORTED();
}

void ChargeInfo::setPlayer(runtime::Ptr<gameobjects::player::Player> player) {
	AION_UNPORTED();
}

bool ChargeInfo::updateChargePoints(int32_t pointsToAdd) {
	AION_UNPORTED();
}

void ChargeInfo::dotattacked(gameobjects::Creature& creature, skillengine::model::Effect& dotEffect) {
	AION_UNPORTED();
}

void ChargeInfo::attacked(gameobjects::Creature& creature, int32_t skillId) {
	AION_UNPORTED();
}

void ChargeInfo::attack(gameobjects::Creature& creature, int32_t skillId) {
	AION_UNPORTED();
}

void ChargeInfo::sendItemUpdate() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items
