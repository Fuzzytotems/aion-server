#pragma once

#include "aion/gameserver/dataholders/PanelSkillsData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PanelSkillsData. @author xTz */
class PanelSkillsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PanelSkillsData.xml.inc"
public:
};

} // namespace aion::gameserver::dataholders
