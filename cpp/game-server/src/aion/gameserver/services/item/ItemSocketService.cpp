#include "aion/gameserver/services/item/ItemSocketService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::item {

runtime::Ptr<model::items::ManaStone> ItemSocketService::addManaStone(runtime::Ptr<model::gameobjects::Item> item, int32_t manaStoneItemId, bool useFusionSlots) {
	AION_UNPORTED();
}

runtime::Ptr<model::items::ManaStone> ItemSocketService::addManaStone(runtime::Ptr<model::gameobjects::Item> item, int32_t manaStoneItemId, int32_t slotId, bool useFusionSlots) {
	AION_UNPORTED();
}

runtime::Ptr<model::items::ManaStone> ItemSocketService::insertManaStoneIntoNextPossibleSlot(model::gameobjects::Item& item, int32_t manaStoneItemId, int32_t maxSlots, runtime::RcTreeSet<runtime::Ref<model::items::ManaStone>>& manaStones, model::templates::item::enums::ItemGroup manastoneCategory, int32_t specialSlotCount) {
	AION_UNPORTED();
}

runtime::Ptr<model::items::ManaStone> ItemSocketService::insertManastoneIntoSlot(model::gameobjects::Item& item, runtime::RcTreeSet<runtime::Ref<model::items::ManaStone>>& manaStones, int32_t manastoneId, int32_t slotId) {
	AION_UNPORTED();
}

void ItemSocketService::copyFusionStones(model::gameobjects::Item& source, model::gameobjects::Item& target) {
	AION_UNPORTED();
}

void ItemSocketService::removeManastone(model::gameobjects::player::Player& player, int32_t itemObjId, int32_t slotNum, bool isFusionSocket) {
	AION_UNPORTED();
}

void ItemSocketService::removeAllManastone(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::Item> item) {
	AION_UNPORTED();
}

// anonymous ItemUseObserver at ItemSocketService.java:177 (fieldmap key ItemSocketService$1); local observer; storage: stored in ObserveController
void ItemSocketService::socketGodstone(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::Item> weapon, int32_t stoneId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::item
