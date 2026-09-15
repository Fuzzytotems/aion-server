#include "aion/gameserver/dataholders/RecipeData.h"

namespace aion::gameserver::dataholders {

void RecipeData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	recipeData.clear();
	for (const model::templates::recipe::RecipeTemplate& it : list)
		recipeData.insert_or_assign(it.getId(), &it);
	// Java also collects the auto-learn recipes here (getAutolearnRecipes: P4-09); list = null (the C++ index points into the storage, which
	// stays)
}

const model::templates::recipe::RecipeTemplate* RecipeData::getRecipeTemplateById(int32_t id) const {
	auto it = recipeData.find(id);
	return it != recipeData.end() ? it->second : nullptr;
}

} // namespace aion::gameserver::dataholders
