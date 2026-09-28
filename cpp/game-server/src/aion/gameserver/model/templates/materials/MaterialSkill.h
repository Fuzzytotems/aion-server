#pragma once

#include <vector>

#include "aion/gameserver/model/templates/materials/MaterialSkill.xml.h"

namespace aion::gameserver::model::templates::materials {

/** Java com.aionemu.gameserver.model.templates.materials.MaterialSkill. @author Rolandas */
class MaterialSkill : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/materials/MaterialSkill.xml.inc"
public:
	/** Java returns Collections.emptyList() without conditions */
	const std::vector<MaterialActCondition>& getConditions() const;

	/** @return MaterialTarget::ALL without a target attribute */
	MaterialTarget getTarget() const { return target.value_or(MaterialTarget::ALL); }
};

} // namespace aion::gameserver::model::templates::materials
