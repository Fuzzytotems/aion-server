#include "aion/gameserver/model/items/detail/StaticDataLookups.h"

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/dataholders/TemperingData.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::model::items::detail {

namespace {

using ItemTemplateLookup = const templates::item::ItemTemplate* (*)(int32_t);
using RandomBonusLookup = const templates::stats::ModifiersTemplate* (*)(templates::item::bonuses::StatBonusType, int32_t, int32_t);
using PolishActionLookup = const templates::item::actions::PolishAction* (*)(const templates::item::actions::ItemActions&);
using CraftLearnActionLookup = const templates::item::actions::CraftLearnAction* (*)(const templates::item::actions::ItemActions&);
using TemperingLookup = const TemperingTemplates* (*)(const templates::item::ItemTemplate*);
using RecipeLookup = const templates::recipe::RecipeTemplate* (*)(int32_t);
using RecipeSkillIdLookup = int32_t (*)(const templates::recipe::RecipeTemplate&);

// setStaticDataLookupsForTests (C++ only test seam); nullptr: the holder or template lookup
runtime::Field<ItemTemplateLookup> itemTemplateLookup{nullptr};
runtime::Field<RandomBonusLookup> randomBonusLookup{nullptr};
runtime::Field<PolishActionLookup> polishActionLookup{nullptr};
runtime::Field<CraftLearnActionLookup> craftLearnActionLookup{nullptr};
runtime::Field<TemperingLookup> temperingLookup{nullptr};
runtime::Field<RecipeLookup> recipeLookup{nullptr};
runtime::Field<RecipeSkillIdLookup> recipeSkillIdLookup{nullptr};

} // namespace

const templates::item::ItemTemplate* getItemTemplate(int32_t itemId) {
	if (ItemTemplateLookup lookup = itemTemplateLookup.get())
		return lookup(itemId);
	return dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
}

const templates::stats::ModifiersTemplate* getRandomBonusTemplate(templates::item::bonuses::StatBonusType type, int32_t statBonusSetId,
	int32_t statBonusId) {
	if (RandomBonusLookup lookup = randomBonusLookup.get())
		return lookup(type, statBonusSetId, statBonusId);
	return dataholders::DataManager::ITEM_RANDOM_BONUSES->getTemplate(type, statBonusSetId, statBonusId);
}

const templates::item::actions::PolishAction* getPolishAction(const templates::item::actions::ItemActions& actions) {
	if (PolishActionLookup lookup = polishActionLookup.get())
		return lookup(actions);
	return actions.getPolishAction();
}

const templates::item::actions::CraftLearnAction* getCraftLearnAction(const templates::item::actions::ItemActions& actions) {
	if (CraftLearnActionLookup lookup = craftLearnActionLookup.get())
		return lookup(actions);
	return actions.getCraftLearnAction();
}

const TemperingTemplates* getTemperingTemplates(const templates::item::ItemTemplate* itemTemplate) {
	if (TemperingLookup lookup = temperingLookup.get())
		return lookup(itemTemplate);
	return dataholders::DataManager::TEMPERING_DATA->getTemplates(itemTemplate);
}

const templates::recipe::RecipeTemplate* getRecipeTemplateById(int32_t id) {
	if (RecipeLookup lookup = recipeLookup.get())
		return lookup(id);
	return dataholders::DataManager::RECIPE_DATA->getRecipeTemplateById(id);
}

int32_t getRecipeSkillId(const templates::recipe::RecipeTemplate& recipeTemplate) {
	if (RecipeSkillIdLookup lookup = recipeSkillIdLookup.get())
		return lookup(recipeTemplate);
	return recipeTemplate.getSkillId();
}

void setStaticDataLookupsForTests(const StaticDataLookupsForTests& lookups) noexcept {
	itemTemplateLookup.set(lookups.itemTemplate);
	randomBonusLookup.set(lookups.randomBonusTemplate);
	polishActionLookup.set(lookups.polishAction);
	craftLearnActionLookup.set(lookups.craftLearnAction);
	temperingLookup.set(lookups.temperingTemplates);
	recipeLookup.set(lookups.recipeTemplateById);
	recipeSkillIdLookup.set(lookups.recipeSkillId);
}

} // namespace aion::gameserver::model::items::detail
