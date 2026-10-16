#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/ItemData.xml.h"
#include "aion/gameserver/dataholders/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.ItemData.
 * <p>
 * C++: the @XmlTransient indexes point into the bound `its` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets the list
 * to null). getItemTemplates returns the templates in Java's HashMap<Integer, ItemTemplate> iteration order, computed by afterUnmarshal.
 * cleanup is post-processing of the unpublished holder (DataManager::init) and gets the cleanup holder Java reads from DataManager.ITEM_CLEAN_UP.
 *
 * @author Luno
 */
class ItemData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ItemData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::item::ItemTemplate*> items;
	std::unordered_map<int32_t, std::vector<const model::templates::item::ItemTemplate*>> manastones;
	std::unordered_map<int32_t, std::vector<const model::templates::item::ItemTemplate*>> ancientManastones;
	/** C++ only: items.values() in Java's HashMap iteration order */
	std::vector<const model::templates::item::ItemTemplate*> itemsInHashOrder;

	using ItemMap = std::unordered_map<int32_t, std::vector<const model::templates::item::ItemTemplate*>>;

	static void add(ItemMap& itemMap, const model::templates::item::ItemTemplate& item, int32_t manastoneLevel);

	static void addStonesForHigherLevels(ItemMap& itemMap, const model::templates::item::ItemTemplate& item, int32_t manastoneLevel);

public:
	/** Applies the restriction cleanup entries (Java DataManager.ITEM_CLEAN_UP) to the item masks. @throws NullPointerException for an unknown item */
	void cleanup(const ItemRestrictionCleanupData& itemCleanUp);

private:
	static void applyCleanup(model::templates::item::ItemTemplate& item, int8_t result, int32_t mask);

public:
	/** @return the item template, nullptr (Java null) for an unknown id */
	const model::templates::item::ItemTemplate* getItemTemplate(int32_t itemId) const;

	/** Java `items.values()` (HashMap iteration order) */
	const std::vector<const model::templates::item::ItemTemplate*>& getItemTemplates() const;

	int32_t size() const;

	/** @return the manastones of the level, nullptr (Java null) if there are none */
	const std::vector<const model::templates::item::ItemTemplate*>* getManastones(int32_t level) const;

	/** @return the ancient manastones of the level, nullptr (Java null) if there are none */
	const std::vector<const model::templates::item::ItemTemplate*>* getAncientManastones(int32_t level) const;
};

} // namespace aion::gameserver::dataholders
