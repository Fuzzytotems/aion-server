#pragma once

#include "aion/gameserver/model/templates/QuestTemplate.xml.h"

namespace aion::gameserver::model::templates {

/** Java com.aionemu.gameserver.model.templates.QuestTemplate. @author MrPoke, vlog, Neon */
class QuestTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/QuestTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates
