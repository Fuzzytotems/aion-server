#include "aion/gameserver/dataholders/EnchantData.h"

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"

namespace aion::gameserver::dataholders {

void EnchantData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::enchants::EnchantList& enchant : enchantList) {
		LevelStats map;
		for (const model::enchants::EnchantTemplateData& data : enchant.getEnchantDatas())
			map.insert_or_assign(data.getLevel(), &data.getEnchantStats());
		templates.insert_or_assign(enchant.getItemGroup(), std::move(map)); // Java puts the new map before filling it; a later list replaces it
	}
	// Java: enchantList = null (the C++ maps point into the storage, which stays)
}

int32_t EnchantData::size() const {
	return static_cast<int32_t>(templates.size());
}

const EnchantData::LevelStats* EnchantData::getTemplates(const model::templates::item::ItemTemplate& itemTemplate) const {
	// Java: itemTemplate.getEnchantName() != null (an absent attribute; the census finds no present empty enchant name)
	auto it = !itemTemplate.getEnchantName().empty() ? templates.find(itemTemplate.getEnchantName())
	                                                 : templates.find(xml::enumName(itemTemplate.getItemGroup()));
	return it != templates.end() ? &it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
