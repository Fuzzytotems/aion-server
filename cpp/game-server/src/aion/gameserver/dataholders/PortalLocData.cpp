#include "aion/gameserver/dataholders/PortalLocData.h"

namespace aion::gameserver::dataholders {

void PortalLocData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const model::templates::portal::PortalLoc& loc : portalLoc)
		portalLocs.insert_or_assign(loc.getLocId(), &loc);
	// Java: portalLoc = null (the C++ index points into the storage, which stays)
}

int32_t PortalLocData::size() const {
	return static_cast<int32_t>(portalLocs.size());
}

const model::templates::portal::PortalLoc* PortalLocData::getPortalLoc(int32_t locId) const {
	auto it = portalLocs.find(locId);
	return it != portalLocs.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
