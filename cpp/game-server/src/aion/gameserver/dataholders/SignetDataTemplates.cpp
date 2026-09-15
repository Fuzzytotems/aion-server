#include "aion/gameserver/dataholders/SignetDataTemplates.h"

namespace aion::gameserver::dataholders {

void SignetDataTemplates::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const skillengine::model::SignetDataTemplate& data : signetDataTemplateList)
		signets.insert_or_assign(data.getSignet(), &data);
	// Java: signetDataTemplateList = null (the C++ index points into the storage, which stays)
}

const skillengine::model::SignetData* SignetDataTemplates::getSignetData(skillengine::model::SignetEnum signet, int32_t level) const {
	auto it = signets.find(signet);
	if (it != signets.end())
		return it->second->getSignetDataForSignetLevel(level);
	return nullptr;
}

int32_t SignetDataTemplates::size() const {
	return static_cast<int32_t>(signets.size());
}

} // namespace aion::gameserver::dataholders
