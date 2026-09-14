#pragma once

#include "aion/gameserver/model/templates/TitleTemplate.xml.h"

namespace aion::gameserver::model::templates {

/** Java com.aionemu.gameserver.model.templates.TitleTemplate. @author xavier */
class TitleTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/TitleTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates
