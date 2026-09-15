#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/RecipeData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.RecipeData.
 * <p>
 * C++: the @XmlTransient index points into the bound `list` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets the list
 * to null). Java's autoLearnRecipes list, getAutolearnRecipes, getRecipeTemplates and size come with the P4-09 port (header request items-5
 * added getRecipeTemplateById).
 *
 * @author ATracer, MrPoke, KID
 */
class RecipeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/RecipeData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::recipe::RecipeTemplate*> recipeData;

public:
	/** @return the recipe template, nullptr (Java null) for an unknown id */
	const model::templates::recipe::RecipeTemplate* getRecipeTemplateById(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
