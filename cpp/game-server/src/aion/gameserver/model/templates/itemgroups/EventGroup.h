#pragma once

#include "aion/gameserver/model/templates/itemgroups/EventGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.EventGroup. @author Pad */
class EventGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/EventGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
