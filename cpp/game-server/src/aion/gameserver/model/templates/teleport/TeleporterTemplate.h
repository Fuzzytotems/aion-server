#pragma once

#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.xml.h"

namespace aion::gameserver::model::templates::teleport {

/** Java com.aionemu.gameserver.model.templates.teleport.TeleporterTemplate. @author orz */
class TeleporterTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::teleport
