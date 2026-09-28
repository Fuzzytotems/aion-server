#include "aion/gameserver/dataholders/InstanceBuffData.h"

namespace aion::gameserver::dataholders {

void InstanceBuffData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::instance_bonusatrr::InstanceBonusAttr& template_ : instanceBonusattr)
		templates.insert_or_assign(template_.getBuffId(), &template_);
	// Java: instanceBonusattr = null (the C++ index points into the storage, which stays)
}

int32_t InstanceBuffData::size() const {
	return static_cast<int32_t>(templates.size());
}

const model::templates::instance_bonusatrr::InstanceBonusAttr* InstanceBuffData::getInstanceBonusattr(int32_t buffId) const {
	auto it = templates.find(buffId);
	return it != templates.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
