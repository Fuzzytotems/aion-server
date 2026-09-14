#pragma once

#include "aion/gameserver/model/templates/item/actions/SkillUseAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.SkillUseAction. @author ATracer */
class SkillUseAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/SkillUseAction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item::actions
