#include "aion/gameserver/dataholders/AIData.h"

namespace aion::gameserver::dataholders {

void AIData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	aiTemplate.clear();
	for (const model::templates::ai::AITemplate& template_ : templates)
		aiTemplate.insert_or_assign(template_.getNpcId(), &template_);
	// Java: templates = null (the C++ index points into the storage, which stays)
}

int32_t AIData::size() const {
	return static_cast<int32_t>(aiTemplate.size());
}

const model::templates::ai::AITemplate* AIData::getAiTemplate(int32_t npcId) const {
	auto it = aiTemplate.find(npcId);
	return it != aiTemplate.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
