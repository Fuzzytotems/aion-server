#include "aion/gameserver/model/items/GodStone.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::model::items {

GodStone::GodStone(gameobjects::Item& parentItem, int32_t activatedCountValue, int32_t itemIdValue,
	const templates::item::GodstoneInfo* godstoneInfoValue, PersistentState state)
	: ItemStone(parentItem.getObjectId(), itemIdValue, 0, state), godstoneInfo(godstoneInfoValue), activatedCount(activatedCountValue) {
}

GodStone::~GodStone() = default;

runtime::Ref<GodStone> GodStone::create(gameobjects::Item& parentItem, int32_t activatedCountValue, int32_t itemIdValue,
	const templates::item::GodstoneInfo* godstoneInfoValue, PersistentState state) {
	return runtime::makeRef<GodStone>(parentItem, activatedCountValue, itemIdValue, godstoneInfoValue, state);
}

void GodStone::increaseActivatedCount() {
	AION_UNPORTED();
}

bool GodStone::tryActivate(bool isMainHandWeapon, gameobjects::Creature& target) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items
