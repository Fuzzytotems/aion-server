#include "aion/gameserver/dataholders/CosmeticItemsData.h"

namespace aion::gameserver::dataholders {

void CosmeticItemsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::cosmeticitems::CosmeticItemTemplate& template_ : templates)
		cosmeticItemTemplates.insert_or_assign(template_.getCosmeticName(), &template_);
	// Java: templates = null (the C++ index points into the storage, which stays)
}

int32_t CosmeticItemsData::size() const {
	return static_cast<int32_t>(cosmeticItemTemplates.size());
}

const model::templates::cosmeticitems::CosmeticItemTemplate* CosmeticItemsData::getCosmeticItemsTemplate(std::string_view str) const {
	auto it = cosmeticItemTemplates.find(str);
	return it != cosmeticItemTemplates.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
