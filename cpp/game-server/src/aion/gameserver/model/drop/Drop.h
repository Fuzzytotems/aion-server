#pragma once

#include "aion/gameserver/model/drop/Drop.xml.h"

namespace aion::gameserver::model::drop {

/** Java com.aionemu.gameserver.model.drop.Drop. @author MrPoke */
class Drop : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/drop/Drop.xml.inc"
private:
	Drop() = default; // Java: private Drop()
};

} // namespace aion::gameserver::model::drop
