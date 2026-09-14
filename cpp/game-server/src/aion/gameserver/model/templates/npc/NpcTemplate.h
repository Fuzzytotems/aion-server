#pragma once

#include "aion/gameserver/model/templates/npc/NpcTemplate.xml.h"

namespace aion::gameserver::model::templates::npc {

/** Java com.aionemu.gameserver.model.templates.npc.NpcTemplate. @author Luno */
class NpcTemplate : public ::aion::gameserver::model::gameobjects::CreatureTemplate {
#include "aion/gameserver/model/templates/npc/NpcTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::npc
