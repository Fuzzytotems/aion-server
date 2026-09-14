#pragma once

#include "aion/gameserver/model/templates/quest/XMLStartCondition.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.XMLStartCondition. @author antness, vlog */
class XMLStartCondition : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/XMLStartCondition.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::quest
