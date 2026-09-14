#pragma once

#include "aion/gameserver/dataholders/SkillTreeData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.SkillTreeData. @author ATracer */
class SkillTreeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/SkillTreeData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
