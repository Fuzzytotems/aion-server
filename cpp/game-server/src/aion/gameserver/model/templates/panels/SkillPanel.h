#pragma once

#include "aion/gameserver/model/templates/panels/SkillPanel.xml.h"

namespace aion::gameserver::model::templates::panels {

/** Java com.aionemu.gameserver.model.templates.panels.SkillPanel. @author xTz */
class SkillPanel : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/panels/SkillPanel.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::panels
