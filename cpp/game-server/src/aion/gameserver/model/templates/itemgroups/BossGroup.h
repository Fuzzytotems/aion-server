#pragma once

#include "aion/gameserver/model/templates/itemgroups/BossGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.BossGroup. @author Rolandas */
class BossGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/BossGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
