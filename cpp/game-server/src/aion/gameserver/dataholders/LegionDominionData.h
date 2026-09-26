#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/LegionDominionData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.LegionDominionData. @author Yeats */
class LegionDominionData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/LegionDominionData.xml.inc"
public:
	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
