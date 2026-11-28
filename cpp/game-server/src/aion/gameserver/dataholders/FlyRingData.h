#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/FlyRingData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.FlyRingData.
 * <p>
 * C++: an absent list is the empty bound vector (Java returns List.of(); upstream 15eaf77b6 removed addAll).
 *
 * @author M@xx
 */
class FlyRingData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/FlyRingData.xml.inc"
public:
	int32_t size() const;

	const std::vector<model::templates::flyring::FlyRingTemplate>& getFlyRingTemplates() const;
};

} // namespace aion::gameserver::dataholders
