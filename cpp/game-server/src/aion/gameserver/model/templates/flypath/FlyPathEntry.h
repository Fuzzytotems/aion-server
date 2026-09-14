#pragma once

#include "aion/gameserver/model/templates/flypath/FlyPathEntry.xml.h"

namespace aion::gameserver::model::templates::flypath {

/** Java com.aionemu.gameserver.model.templates.flypath.FlyPathEntry. @author KID */
class FlyPathEntry : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/flypath/FlyPathEntry.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::flypath
