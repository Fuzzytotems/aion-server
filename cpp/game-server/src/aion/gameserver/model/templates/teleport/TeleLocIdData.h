#pragma once

#include "aion/gameserver/model/templates/teleport/TeleLocIdData.xml.h"

namespace aion::gameserver::model::templates::teleport {

/** Java com.aionemu.gameserver.model.templates.teleport.TeleLocIdData. @author ATracer */
class TeleLocIdData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/teleport/TeleLocIdData.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::teleport
