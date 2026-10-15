#pragma once

#include "aion/gameserver/model/templates/ai/SummonGroup.xml.h"

namespace aion::gameserver::model::templates::ai {

/** Java com.aionemu.gameserver.model.templates.ai.SummonGroup. @author xTz */
class SummonGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/ai/SummonGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::ai
