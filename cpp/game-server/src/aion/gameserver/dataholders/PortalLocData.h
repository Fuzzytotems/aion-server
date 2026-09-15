#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/PortalLocData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.PortalLocData.
 * <p>
 * C++: the index points into the bound `portalLoc` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author xTz
 */
class PortalLocData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PortalLocData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::portal::PortalLoc*> portalLocs;

public:
	int32_t size() const;

	/** @return the portal location, nullptr (Java null) if there is none */
	const model::templates::portal::PortalLoc* getPortalLoc(int32_t locId) const;
};

} // namespace aion::gameserver::dataholders
