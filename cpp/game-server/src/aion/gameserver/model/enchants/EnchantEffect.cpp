#include "aion/gameserver/model/enchants/EnchantEffect.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/items/ItemSlot.h"

namespace aion::gameserver::model::enchants {

EnchantEffect::EnchantEffect(gameobjects::Item& item, gameobjects::player::Player& player, const std::vector<const EnchantStat*>& enchantStats)
	: itemSlot() {
	// Java: builds a StatAddFunction per enchant stat (itemSlot from the item's equipment slot), then player.getGameStats().addEffect(this, ...)
	AION_UNPORTED();
}

EnchantEffect::~EnchantEffect() = default;

runtime::Ref<EnchantEffect> EnchantEffect::create(gameobjects::Item& item, gameobjects::player::Player& player,
	const std::vector<const EnchantStat*>& enchantStats) {
	return runtime::makeRef<EnchantEffect>(item, player, enchantStats);
}

void EnchantEffect::endEffect(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::enchants
