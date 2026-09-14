#pragma once

#include "aion/gameserver/model/templates/itemgroups/GatherGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.GatherGroup. @author Rolandas */
class GatherGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/GatherGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
