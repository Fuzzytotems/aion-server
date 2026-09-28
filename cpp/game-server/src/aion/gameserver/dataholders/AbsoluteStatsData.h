#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/AbsoluteStatsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.AbsoluteStatsData.
 * <p>
 * C++: the @XmlTransient index points into the bound `absoluteStats` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author Rolandas
 */
class AbsoluteStatsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/AbsoluteStatsData.xml.inc"
private:
	/** Java Map<Integer, ModifiersTemplate>: a stats set without <modifiers> maps to nullptr (Java null) */
	std::unordered_map<int32_t, const model::templates::stats::ModifiersTemplate*> absoluteStatsData;

public:
	/** @return the modifiers of the stats set, nullptr (Java null) if there is none */
	const model::templates::stats::ModifiersTemplate* getTemplate(int32_t statSetId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
