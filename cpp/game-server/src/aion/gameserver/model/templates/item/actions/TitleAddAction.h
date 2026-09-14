#pragma once

#include "aion/gameserver/model/templates/item/actions/TitleAddAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.TitleAddAction. @author Hilgert */
class TitleAddAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/TitleAddAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
