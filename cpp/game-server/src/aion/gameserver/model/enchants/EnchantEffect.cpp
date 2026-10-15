#include "aion/gameserver/model/enchants/EnchantEffect.h"

#include "aion/gameserver/model/enchants/EnchantStat.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::enchants {

namespace {

using stats::container::StatEnum;

/** Java: itemSlot == MAIN_HAND or MAIN_OR_SUB (the enchanted weapon is the main hand weapon) */
bool isMainHand(int64_t itemSlot) noexcept {
	return itemSlot == items::getSlotIdMask(items::ItemSlot::MAIN_HAND) || itemSlot == items::getSlotIdMask(items::ItemSlot::MAIN_OR_SUB);
}

const EnchantStat& requireStat(const EnchantStat* enchantStat) {
	if (enchantStat == nullptr)
		throw runtime::NullPointerException("enchantStat");
	return *enchantStat;
}

/**
 * Java: the itemSlot the constructor assigns for each PHYSICAL_ATTACK or MAGICAL_ATTACK stat (the same value each time).
 * Deviation: Java leaves the field null without such a stat; the frozen member is a plain ItemSlot, so it is MAIN_HAND then. Only the //stat
 * admin command prints it for those effects; CreatureGameStats reads it only for attack stats (docs/deviations/P4-13.md).
 */
items::ItemSlot itemSlotOf(gameobjects::Item& item, const std::vector<const EnchantStat*>& enchantStats) {
	int64_t itemSlot = item.getEquipmentSlot();
	for (const EnchantStat* enchantStat : enchantStats) {
		StatEnum stat = requireStat(enchantStat).getStat();
		if (stat == StatEnum::PHYSICAL_ATTACK || stat == StatEnum::MAGICAL_ATTACK)
			return isMainHand(itemSlot) ? items::ItemSlot::MAIN_HAND : items::ItemSlot::SUB_HAND;
	}
	return items::ItemSlot::MAIN_HAND;
}

} // namespace

EnchantEffect::EnchantEffect(gameobjects::Item& item, gameobjects::player::Player& player, const std::vector<const EnchantStat*>& enchantStats)
	: itemSlot(itemSlotOf(item, enchantStats)) {
	std::vector<runtime::Ref<stats::calc::functions::IStatFunction>> functions;
	int64_t equipmentSlot = item.getEquipmentSlot();
	for (const EnchantStat* stat : enchantStats) {
		const EnchantStat& enchantStat = requireStat(stat);
		switch (enchantStat.getStat()) {
			case StatEnum::PHYSICAL_ATTACK:
			case StatEnum::MAGICAL_ATTACK:
				functions.push_back(
					stats::calc::functions::RcStatFunction<stats::calc::functions::StatAddFunction>::create(enchantStat.getStat(), enchantStat.getValue(), false));
				break;
			case StatEnum::BOOST_MAGICAL_SKILL:
				if (isMainHand(equipmentSlot))
					functions.push_back(stats::calc::functions::RcStatFunction<stats::calc::functions::StatAddFunction>::create(enchantStat.getStat(),
						enchantStat.getValue(), false));
				break;
			default:
				functions.push_back(
					stats::calc::functions::RcStatFunction<stats::calc::functions::StatAddFunction>::create(enchantStat.getStat(), enchantStat.getValue(), false));
				break;
		}
	}
	std::vector<runtime::Ptr<stats::calc::functions::IStatFunction>> borrowed(functions.begin(), functions.end());
	player.getGameStats()->addEffect(runtime::Ptr<stats::calc::StatOwner>(static_cast<stats::calc::StatOwner&>(*this)), borrowed);
}

EnchantEffect::~EnchantEffect() = default;

runtime::Ref<EnchantEffect> EnchantEffect::create(gameobjects::Item& item, gameobjects::player::Player& player,
	const std::vector<const EnchantStat*>& enchantStats) {
	return runtime::makeRef<EnchantEffect>(item, player, enchantStats);
}

void EnchantEffect::endEffect(gameobjects::player::Player& player) {
	player.getGameStats()->endEffect(*this);
}

} // namespace aion::gameserver::model::enchants
