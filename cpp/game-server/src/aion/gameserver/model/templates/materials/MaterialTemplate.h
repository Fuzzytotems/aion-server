#pragma once

#include <vector>

#include "aion/gameserver/model/templates/materials/MaterialTemplate.xml.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::model::templates::materials {

/** Java com.aionemu.gameserver.model.templates.materials.MaterialTemplate (a behaviour class since upstream ea7b1c32a). @author Rolandas */
class MaterialTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/materials/MaterialTemplate.xml.inc"
public:
	/** @return the skills whose target matches the creature, in data order (Java: a new list) */
	std::vector<const MaterialSkill*> getSkills(gameobjects::Creature& creature) const;
};

} // namespace aion::gameserver::model::templates::materials
