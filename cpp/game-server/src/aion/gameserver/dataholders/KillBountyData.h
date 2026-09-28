#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/KillBountyData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.KillBountyData. @author Estrayl */
class KillBountyData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/KillBountyData.xml.inc"
public:
	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
