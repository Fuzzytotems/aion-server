#pragma once

#include <cstdint>
#include <unordered_set>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/templates/item/enums/fwd.h"
#include "aion/gameserver/services/item/fwd.h"

namespace aion::gameserver::services::item {

/**
 * C++: a static-only class (hub-headers.md §11.1). manaStones is the item's live stone set that insertManastoneIntoSlot adds to (RcTreeSet&).
 *
 * @author ATracer, Sykra
 */
class ItemSocketService {
public:
	static runtime::Ptr<model::items::ManaStone> addManaStone(runtime::Ptr<model::gameobjects::Item> item, int32_t manaStoneItemId, bool useFusionSlots);
	static runtime::Ptr<model::items::ManaStone> addManaStone(runtime::Ptr<model::gameobjects::Item> item, int32_t manaStoneItemId, int32_t slotId, bool useFusionSlots);
private:
	static runtime::Ptr<model::items::ManaStone> insertManaStoneIntoNextPossibleSlot(model::gameobjects::Item& item, int32_t manaStoneItemId, int32_t maxSlots, runtime::RcTreeSet<runtime::Ref<model::items::ManaStone>>& manaStones, model::templates::item::enums::ItemGroup manastoneCategory, int32_t specialSlotCount);
	static runtime::Ptr<model::items::ManaStone> insertManastoneIntoSlot(model::gameobjects::Item& item, runtime::RcTreeSet<runtime::Ref<model::items::ManaStone>>& manaStones, int32_t manastoneId, int32_t slotId);
public:
	static void copyFusionStones(model::gameobjects::Item& source, model::gameobjects::Item& target);
	static void removeManastone(model::gameobjects::player::Player& player, int32_t itemObjId, int32_t slotNum, bool isFusionSocket);
	static void removeAllManastone(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::Item> item);
	static void socketGodstone(model::gameobjects::player::Player& player, runtime::Ptr<model::gameobjects::Item> weapon, int32_t stoneId);
};

} // namespace aion::gameserver::services::item
