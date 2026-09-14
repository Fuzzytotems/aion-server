#pragma once

#include "aion/gameserver/model/gameobjects/CreatureTemplate.xml.h"

namespace aion::gameserver::model::gameobjects {

/** Java com.aionemu.gameserver.model.gameobjects.CreatureTemplate. @author Neon */
class CreatureTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/gameobjects/CreatureTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::gameobjects
