#include "aion/gameserver/dataholders/RecipeData.h"

#include "aion/gameserver/dataholders/detail/JavaHashMapOrder.h"
#include "aion/gameserver/model/Race.h"

namespace aion::gameserver::dataholders {

using model::templates::recipe::RecipeTemplate;

void RecipeData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	recipeData.clear();
	detail::JavaHashMapOrder<int32_t, const RecipeTemplate*> order;
	for (const RecipeTemplate& it : list) {
		recipeData.insert_or_assign(it.getId(), &it);
		order.put(it.getId(), &it, detail::javaHashCode(it.getId()));
		if (it.getAutoLearn() != 0)
			autoLearnRecipes.push_back(&it);
	}
	recipesInHashOrder = order.values();
	// Java: list = null (the C++ index points into the storage, which stays)
}

std::vector<const RecipeTemplate*> RecipeData::getAutolearnRecipes(model::Race race, int32_t skillId, int32_t maxLevel) const {
	std::vector<const RecipeTemplate*> result;
	for (const RecipeTemplate* recipe : autoLearnRecipes) {
		if (recipe->getSkillId() != skillId || recipe->getSkillpoint() > maxLevel)
			continue;
		// Java: recipe.getRace() != Race.PC_ALL && recipe.getRace() != race (a null race is neither)
		if (recipe->getRace() != model::Race::PC_ALL && recipe->getRace() != race)
			continue;
		result.push_back(recipe);
	}
	return result;
}

const RecipeTemplate* RecipeData::getRecipeTemplateById(int32_t id) const {
	auto it = recipeData.find(id);
	return it != recipeData.end() ? it->second : nullptr;
}

const std::vector<const RecipeTemplate*>& RecipeData::getRecipeTemplates() const {
	return recipesInHashOrder;
}

int32_t RecipeData::size() const {
	return static_cast<int32_t>(recipeData.size());
}

} // namespace aion::gameserver::dataholders
