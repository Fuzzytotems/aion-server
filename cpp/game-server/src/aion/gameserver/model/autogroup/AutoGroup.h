#pragma once

#include "aion/gameserver/model/autogroup/AutoGroup.xml.h"

namespace aion::gameserver::model::autogroup {

/** Java com.aionemu.gameserver.model.autogroup.AutoGroup. @author MrPoke */
class AutoGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/autogroup/AutoGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::autogroup
