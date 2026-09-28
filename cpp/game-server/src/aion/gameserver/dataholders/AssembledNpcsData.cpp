#include "aion/gameserver/dataholders/AssembledNpcsData.h"

namespace aion::gameserver::dataholders {

void AssembledNpcsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::assemblednpc::AssembledNpcTemplate& template_ : templates)
		assembledNpcsTemplates.insert_or_assign(template_.getNr(), &template_);
	// Java: templates = null (the C++ index points into the storage, which stays)
}

int32_t AssembledNpcsData::size() const {
	return static_cast<int32_t>(assembledNpcsTemplates.size());
}

const model::templates::assemblednpc::AssembledNpcTemplate* AssembledNpcsData::getAssembledNpcTemplate(int32_t i) const {
	auto it = assembledNpcsTemplates.find(i);
	return it != assembledNpcsTemplates.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
