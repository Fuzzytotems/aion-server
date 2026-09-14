#pragma once

#include "aion/gameserver/dataholders/SkillChargeData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.SkillChargeData. @author Rolandas */
class SkillChargeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SkillChargeData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
