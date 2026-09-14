#pragma once

#include "aion/gameserver/model/templates/itemgroups/MedalGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.MedalGroup. @author Luzien */
class MedalGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/MedalGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
