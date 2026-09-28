#include "aion/gameserver/dataholders/TemperingData.h"

#include <string>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

void TemperingData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	templates.clear();
	for (const model::enchants::TemperingList& tempering : temperingList) {
		auto& map = templates[tempering.getItemGroup()];
		map.clear(); // Java: templates.put(itemGroup, new HashMap<>()) replaces an earlier map of the same group
		for (const model::enchants::TemperingTemplateData& data : tempering.getTemperingDatas())
			map.insert_or_assign(data.getLevel(), &data.getTemperingStats());
	}
	// Java: temperingList = null (the C++ maps point into the storage, which stays)
}

const std::unordered_map<int32_t, const std::vector<model::enchants::TemperingStat>*>*
TemperingData::getTemplates(const model::templates::item::ItemTemplate* itemTemplate) const {
	if (itemTemplate == nullptr)
		throw runtime::NullPointerException("itemTemplate");
	// Java: getTemperingName() != null; an absent attribute binds as an empty string (static-data.md §2.4)
	auto it = !itemTemplate->getTemperingName().empty() ? templates.find(itemTemplate->getTemperingName())
	                                                    : templates.find(xml::enumName(itemTemplate->getItemGroup()));
	return it != templates.end() ? &it->second : nullptr;
}

int32_t TemperingData::size() const {
	return static_cast<int32_t>(templates.size());
}

} // namespace aion::gameserver::dataholders
