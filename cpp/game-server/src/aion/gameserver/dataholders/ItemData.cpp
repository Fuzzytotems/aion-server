#include "aion/gameserver/dataholders/ItemData.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/items/ItemMask.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

using model::templates::item::ItemQuality;
using model::templates::item::ItemTemplate;
using model::templates::item::enums::ItemGroup;

void ItemData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	items.clear();
	detail::JavaHashMapOrder<int32_t, const ItemTemplate*> order;
	for (const ItemTemplate& it : its) {
		items.insert_or_assign(it.getTemplateId(), &it);
		order.put(it.getTemplateId(), &it, detail::javaHashCode(it.getTemplateId()));
		if (it.getItemGroup() == ItemGroup::MANASTONE) {
			add(manastones, it, it.getLevel());
			addStonesForHigherLevels(manastones, it, it.getLevel());
		} else if (it.getItemGroup() == ItemGroup::SPECIAL_MANASTONE) {
			if (commons::utils::StringUtils::toLowerCase(it.getName()).find("pvp") == std::string::npos)
				add(ancientManastones, it, it.getLevel());
		}
	}
	itemsInHashOrder = order.values();
	// Java: its = null (the C++ indexes point into the storage, which stays)
}

void ItemData::add(ItemMap& itemMap, const ItemTemplate& item, int32_t manastoneLevel) {
	if (item.getName().starts_with("[")) // skip [Stamp], [Event], [Legion], [Legion reward], ...
		return;
	itemMap[manastoneLevel].push_back(&item);
}

void ItemData::addStonesForHigherLevels(ItemMap& itemMap, const ItemTemplate& item, int32_t manastoneLevel) {
	if (manastoneLevel == 60 && item.getTemplateId() == 167000563) { // Manastone: Healing Boost +3
		add(itemMap, item, 70);
	} else if (manastoneLevel == 10 || manastoneLevel == 30 || manastoneLevel == 50) {
		if (item.getItemQuality() == ItemQuality::COMMON || item.getItemQuality() == ItemQuality::RARE) {
			if (item.getName().find("Manastone: Attack +") != std::string::npos) {
				add(itemMap, item, manastoneLevel + 10); // add 10 as 20 / 30 as 40 / 50 as 60
				if (manastoneLevel == 50)
					add(itemMap, item, 70);
			}
		}
	}
}

void ItemData::cleanup(const ItemRestrictionCleanupData& itemCleanUp) {
	for (const model::templates::restriction::ItemCleanupTemplate& ict : itemCleanUp.getList()) {
		// the template belongs to this unpublished holder (post-processing, static-data.md §3.3)
		auto* template_ = const_cast<ItemTemplate*>(getItemTemplate(ict.getId()));
		if (template_ == nullptr)
			throw runtime::NullPointerException("Cannot invoke \"ItemTemplate.modifyMask(boolean, int)\" because \"item\" is null (item " +
			                                    std::to_string(ict.getId()) + ")");
		applyCleanup(*template_, ict.resultTrade(), model::items::ItemMask::TRADEABLE);
		applyCleanup(*template_, ict.resultSell(), model::items::ItemMask::SELLABLE);
		applyCleanup(*template_, ict.resultWH(), model::items::ItemMask::STORABLE_IN_WH);
		applyCleanup(*template_, ict.resultAccountWH(), model::items::ItemMask::STORABLE_IN_AWH);
		applyCleanup(*template_, ict.resultLegionWH(), model::items::ItemMask::STORABLE_IN_LWH);
	}
}

void ItemData::applyCleanup(ItemTemplate& item, int8_t result, int32_t mask) {
	switch (result) {
		case 1:
			item.modifyMask(true, mask);
			break;
		case 0:
			item.modifyMask(false, mask);
			break;
		default:
			break;
	}
}

const ItemTemplate* ItemData::getItemTemplate(int32_t itemId) const {
	auto it = items.find(itemId);
	return it != items.end() ? it->second : nullptr;
}

const std::vector<const ItemTemplate*>& ItemData::getItemTemplates() const {
	return itemsInHashOrder;
}

int32_t ItemData::size() const {
	return static_cast<int32_t>(items.size());
}

const std::vector<const ItemTemplate*>* ItemData::getManastones(int32_t level) const {
	auto it = manastones.find(level);
	return it != manastones.end() ? &it->second : nullptr;
}

const std::vector<const ItemTemplate*>* ItemData::getAncientManastones(int32_t level) const {
	auto it = ancientManastones.find(level);
	return it != ancientManastones.end() ? &it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
