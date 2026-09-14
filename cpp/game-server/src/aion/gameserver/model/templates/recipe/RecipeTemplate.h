#pragma once

#include "aion/gameserver/model/templates/recipe/RecipeTemplate.xml.h"

namespace aion::gameserver::model::templates::recipe {

/** Java com.aionemu.gameserver.model.templates.recipe.RecipeTemplate. @author ATracer */
class RecipeTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::recipe
