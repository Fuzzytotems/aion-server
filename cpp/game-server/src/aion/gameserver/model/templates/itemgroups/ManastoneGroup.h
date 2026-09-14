#pragma once

#include "aion/gameserver/model/templates/itemgroups/ManastoneGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.ManastoneGroup. @author Rolandas */
class ManastoneGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/ManastoneGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
