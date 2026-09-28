#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/model/enchants/fwd.h"
#include "aion/gameserver/model/templates/item/actions/fwd.h"
#include "aion/gameserver/model/templates/item/bonuses/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/recipe/fwd.h"
#include "aion/gameserver/model/templates/stats/fwd.h"

namespace aion::gameserver::model::items::detail {

/**
 * C++ only, private to P4-13 (model.items, model.trade, model.drop, model.enchants, model.broker): the static data lookups of the item model
 * bodies. Each function is the Java expression named in its comment (header requests items-1 to items-6 added the holder and template
 * declarations).
 * <p>
 * Test seam: while a test has installed a lookup with setStaticDataLookupsForTests, the function calls it; otherwise it evaluates the Java
 * expression (the published holders: NullPointerException while one is not published). Server code never installs lookups.
 */

/** Java Map<Integer, List<TemperingStat>> of TemperingData.getTemplates: tempering level -> the stats of that level (static data) */
using TemperingTemplates = std::unordered_map<int32_t, const std::vector<enchants::TemperingStat>*>;

/** Java DataManager.ITEM_DATA.getItemTemplate(itemId): nullptr for an unknown id */
const templates::item::ItemTemplate* getItemTemplate(int32_t itemId);

/**
 * Java DataManager.ITEM_RANDOM_BONUSES.getTemplate(type, statBonusSetId, statBonusId): nullptr for a missing bonus set.
 * @throws IndexOutOfBoundsException for a statBonusId outside 1..size of an existing set (Java List.get(statBonusId - 1))
 */
const templates::stats::ModifiersTemplate* getRandomBonusTemplate(templates::item::bonuses::StatBonusType type, int32_t statBonusSetId,
	int32_t statBonusId);

/** Java itemActions.getPolishAction(): nullptr if there is none */
const templates::item::actions::PolishAction* getPolishAction(const templates::item::actions::ItemActions& actions);

/** Java itemActions.getCraftLearnAction(): nullptr if there is none */
const templates::item::actions::CraftLearnAction* getCraftLearnAction(const templates::item::actions::ItemActions& actions);

/** Java DataManager.TEMPERING_DATA.getTemplates(itemTemplate): nullptr (Java null) if there are none */
const TemperingTemplates* getTemperingTemplates(const templates::item::ItemTemplate* itemTemplate);

/** Java DataManager.RECIPE_DATA.getRecipeTemplateById(id): nullptr for an unknown id */
const templates::recipe::RecipeTemplate* getRecipeTemplateById(int32_t id);

/** Java recipeTemplate.getSkillId() */
int32_t getRecipeSkillId(const templates::recipe::RecipeTemplate& recipeTemplate);

/** Test lookups; a null member keeps the holder or template lookup for that function */
struct StaticDataLookupsForTests {
	const templates::item::ItemTemplate* (*itemTemplate)(int32_t itemId) = nullptr;
	const templates::stats::ModifiersTemplate* (*randomBonusTemplate)(templates::item::bonuses::StatBonusType type, int32_t statBonusSetId,
		int32_t statBonusId) = nullptr;
	const templates::item::actions::PolishAction* (*polishAction)(const templates::item::actions::ItemActions& actions) = nullptr;
	const templates::item::actions::CraftLearnAction* (*craftLearnAction)(const templates::item::actions::ItemActions& actions) = nullptr;
	const TemperingTemplates* (*temperingTemplates)(const templates::item::ItemTemplate* itemTemplate) = nullptr;
	const templates::recipe::RecipeTemplate* (*recipeTemplateById)(int32_t id) = nullptr;
	int32_t (*recipeSkillId)(const templates::recipe::RecipeTemplate& recipeTemplate) = nullptr;
};

/** Tests only: installs the lookups (a default-constructed argument removes them) */
void setStaticDataLookupsForTests(const StaticDataLookupsForTests& lookups) noexcept;

} // namespace aion::gameserver::model::items::detail
