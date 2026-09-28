#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/GatherableData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.GatherableData.
 * <p>
 * C++: the index points into the bound `gatherables` storage, which stays after afterUnmarshal (static-data.md §2.6). The hook sorts the bound
 * material lists in place before the holder is published (Java List.sort, stable, by Material.compareTo).
 *
 * @author ATracer
 */
class GatherableData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/GatherableData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::gather::GatherableTemplate*> gatherableData;

public:
	int32_t size() const;

	/** @return the gatherable template, nullptr (Java null) if there is none */
	const model::templates::gather::GatherableTemplate* getGatherableTemplate(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
