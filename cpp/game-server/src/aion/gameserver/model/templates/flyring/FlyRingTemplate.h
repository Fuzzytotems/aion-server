#pragma once

#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.xml.h"

namespace aion::gameserver::model::templates::flyring {

/** Java com.aionemu.gameserver.model.templates.flyring.FlyRingTemplate. @author M@xx */
class FlyRingTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/flyring/FlyRingTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::flyring
