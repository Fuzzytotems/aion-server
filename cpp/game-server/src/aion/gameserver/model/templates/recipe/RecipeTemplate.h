#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/recipe/RecipeTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::recipe {

/** Java com.aionemu.gameserver.model.templates.recipe.RecipeTemplate. @author ATracer */
class RecipeTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameid; }
};

} // namespace aion::gameserver::model::templates::recipe
