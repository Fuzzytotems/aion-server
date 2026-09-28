#pragma once

#include <functional>
#include <initializer_list>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/stats/listeners/fwd.h"
#include "aion/gameserver/model/templates/itemset/fwd.h"

namespace aion::gameserver::model::stats::listeners {

/**
 * Adds and removes the stat functions of an item (modifiers, item set bonuses, stones, enchantment, tempering) when it is equipped or unequipped.
 * <p>
 * C++: a static-only class. Java's `Set<? extends ManaStone>` parameters (the item's TreeSet of stones) are `std::vector<Ptr<ManaStone>>` in the
 * set's iteration order, so the stat functions are added in Java's order. `List<StatFunction>` of static data modifiers are
 * `std::vector<const StatFunction*>`.
 *
 * @author xavier, Wakizashi
 */
class ItemEquipmentListener {
public:
	ItemEquipmentListener() = delete;

	static void onItemEquipment(gameobjects::Item& item, gameobjects::player::Player& owner);

private:
	static void forEachBonusStats(const std::function<void(items::RandomBonusEffect&)>& action,
		std::initializer_list<runtime::Ptr<items::RandomBonusEffect>> bonusStatsEffects = {});

public:
	static void onItemUnequipment(gameobjects::Item& item, gameobjects::player::Player& owner);

private:
	static void addWeaponStats(gameobjects::Item& item, container::CreatureGameStats& cgs);

	static std::vector<const calc::functions::StatFunction*> extractApplicableWeaponModifiers(gameobjects::Item& item,
		const std::vector<const calc::functions::StatFunction*>& modifiers);

	/** @param itemSetTemplate nullable (Java checks null) */
	static void recalculateItemSet(const templates::itemset::ItemSetTemplate* itemSetTemplate, gameobjects::player::Player& player);

	/** @param itemStones Java Set<ManaStone> (nullable: an empty vector), in the set's iteration order */
	static void addStonesStats(gameobjects::Item& item, const std::vector<runtime::Ptr<items::ManaStone>>& itemStones,
		container::CreatureGameStats& cgs);

public:
	/** @param stone nullable (Java checks null) */
	static void addStoneStats(gameobjects::Item& item, runtime::Ptr<items::ManaStone> stone, container::CreatureGameStats& cgs);

	/** @param itemStones Java Set<ManaStone> (nullable: an empty vector), in the set's iteration order */
	static void removeStoneStats(const std::vector<runtime::Ptr<items::ManaStone>>& itemStones, container::CreatureGameStats& cgs);
};

} // namespace aion::gameserver::model::stats::listeners
