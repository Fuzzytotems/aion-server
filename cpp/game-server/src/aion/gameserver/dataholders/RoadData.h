#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/RoadData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.RoadData.
 * <p>
 * C++: an absent list is the empty bound vector (Java returns List.of(); upstream 15eaf77b6 removed addAll).
 *
 * @author SheppeR
 */
class RoadData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/RoadData.xml.inc"
public:
	int32_t size() const;

	const std::vector<model::templates::road::RoadTemplate>& getRoadTemplates() const;
};

} // namespace aion::gameserver::dataholders
