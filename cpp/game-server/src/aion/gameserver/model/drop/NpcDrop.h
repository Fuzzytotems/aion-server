#pragma once

#include "aion/gameserver/model/drop/NpcDrop.xml.h"

namespace aion::gameserver::model::drop {

/** Java com.aionemu.gameserver.model.drop.NpcDrop. @author MrPoke */
class NpcDrop : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/drop/NpcDrop.xml.inc"
public:
};

} // namespace aion::gameserver::model::drop
