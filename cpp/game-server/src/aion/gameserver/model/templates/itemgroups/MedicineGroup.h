#pragma once

#include "aion/gameserver/model/templates/itemgroups/MedicineGroup.xml.h"

namespace aion::gameserver::model::templates::itemgroups {

/** Java com.aionemu.gameserver.model.templates.itemgroups.MedicineGroup. @author Rolandas */
class MedicineGroup : public ::aion::gameserver::model::templates::itemgroups::BonusItemGroup {
#include "aion/gameserver/model/templates/itemgroups/MedicineGroup.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::itemgroups
