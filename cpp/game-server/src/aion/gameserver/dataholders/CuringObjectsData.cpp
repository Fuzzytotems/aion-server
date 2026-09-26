#include "aion/gameserver/dataholders/CuringObjectsData.h"

namespace aion::gameserver::dataholders {

void CuringObjectsData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::curingzones::CuringTemplate& template_ : curingObject)
		curingObjects.push_back(&template_);
}

int32_t CuringObjectsData::size() const {
	return static_cast<int32_t>(curingObjects.size());
}

const std::vector<const model::templates::curingzones::CuringTemplate*>& CuringObjectsData::getCuringObject() const {
	return curingObjects;
}

} // namespace aion::gameserver::dataholders
