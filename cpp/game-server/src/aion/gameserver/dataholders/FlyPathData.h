#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/FlyPathData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.FlyPathData.
 * <p>
 * C++: the index points into the bound `list` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author KID
 */
class FlyPathData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/FlyPathData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::flypath::FlyPathEntry*> loctlistData;

public:
	int32_t size() const;

	/** @return the fly path, nullptr (Java null) if there is none */
	const model::templates::flypath::FlyPathEntry* getPathTemplate(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
