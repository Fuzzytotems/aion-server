#pragma once

#include "aion/gameserver/model/templates/flyring/FlyRingPoint.xml.h"

namespace aion::gameserver::model::templates::flyring {

/** Java com.aionemu.gameserver.model.templates.flyring.FlyRingPoint. @author M@xx */
class FlyRingPoint : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/flyring/FlyRingPoint.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::flyring
