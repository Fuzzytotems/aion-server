#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/ConquerorAndProtectorData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.ConquerorAndProtectorData. @author Dtem */
class ConquerorAndProtectorData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/ConquerorAndProtectorData.xml.inc"
public:
	/** @return the first rank of the type with the rank number, nullptr (Java null) if there is none */
	const model::templates::cp::CPRank* getRank(model::templates::cp::CPType type, int32_t rank) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
