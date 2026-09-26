#include "aion/gameserver/dataholders/TeleLocationData.h"

namespace aion::gameserver::dataholders {

void TeleLocationData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::teleport::TelelocationTemplate& loc : tlist)
		loctlistData.insert_or_assign(loc.getLocId(), &loc);
	// Java: tlist = null (the C++ index points into the storage, which stays)
}

int32_t TeleLocationData::size() const {
	return static_cast<int32_t>(loctlistData.size());
}

const model::templates::teleport::TelelocationTemplate* TeleLocationData::getTelelocationTemplate(int32_t id) const {
	auto it = loctlistData.find(id);
	return it != loctlistData.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
