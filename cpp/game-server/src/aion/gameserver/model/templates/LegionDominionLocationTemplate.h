#pragma once

#include "aion/gameserver/model/templates/LegionDominionLocationTemplate.xml.h"

namespace aion::gameserver::model::templates {

/** Java com.aionemu.gameserver.model.templates.LegionDominionLocationTemplate. @author Yeats, Sykra */
class LegionDominionLocationTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/LegionDominionLocationTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates
