#pragma once

#include "aion/gameserver/model/templates/materials/MaterialSkill.xml.h"

namespace aion::gameserver::model::templates::materials {

/** Java com.aionemu.gameserver.model.templates.materials.MaterialSkill. @author Rolandas */
class MaterialSkill : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/materials/MaterialSkill.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::materials
