#pragma once

#include "aion/gameserver/model/templates/item/actions/AdoptPetAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.AdoptPetAction. @author Rolandas */
class AdoptPetAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/AdoptPetAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
