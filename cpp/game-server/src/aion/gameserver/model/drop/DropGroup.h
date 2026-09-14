#pragma once

#include "aion/gameserver/model/drop/DropGroup.xml.h"

namespace aion::gameserver::model::drop {

/** Java com.aionemu.gameserver.model.drop.DropGroup. @author MrPoke */
class DropGroup : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/drop/DropGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::drop
