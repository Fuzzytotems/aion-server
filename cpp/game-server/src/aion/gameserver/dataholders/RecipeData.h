#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/RecipeData.xml.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.RecipeData.
 * <p>
 * C++: the @XmlTransient index and list point into the bound `list` storage, which stays after afterUnmarshal (static-data.md §2.6; Java sets
 * the list to null). getRecipeTemplates returns the recipes in Java's HashMap<Integer, RecipeTemplate> iteration order.
 *
 * @author ATracer, MrPoke, KID
 */
class RecipeData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/RecipeData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::recipe::RecipeTemplate*> recipeData;
	std::vector<const model::templates::recipe::RecipeTemplate*> autoLearnRecipes;
	/** C++ only: recipeData.values() in Java's HashMap iteration order */
	std::vector<const model::templates::recipe::RecipeTemplate*> recipesInHashOrder;

public:
	std::vector<const model::templates::recipe::RecipeTemplate*> getAutolearnRecipes(model::Race race, int32_t skillId, int32_t maxLevel) const;

	/** @return the recipe template, nullptr (Java null) for an unknown id */
	const model::templates::recipe::RecipeTemplate* getRecipeTemplateById(int32_t id) const;

	/** Java `recipeData.values()` (HashMap iteration order) */
	const std::vector<const model::templates::recipe::RecipeTemplate*>& getRecipeTemplates() const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
