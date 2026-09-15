#include "aion/gameserver/model/templates/materials/MaterialSkill.h"

namespace aion::gameserver::model::templates::materials {

const std::vector<MaterialActCondition>& MaterialSkill::getConditions() const {
	static const std::vector<MaterialActCondition> empty;
	return conditions ? *conditions : empty;
}

} // namespace aion::gameserver::model::templates::materials
