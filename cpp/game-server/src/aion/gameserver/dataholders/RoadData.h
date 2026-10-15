#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/RoadData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.RoadData.
 * <p>
 * C++: an absent list is the empty bound vector (Java creates an empty list lazily). addAll has no caller in Java and would append to the
 * immutable published holder, so it is not declared.
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
