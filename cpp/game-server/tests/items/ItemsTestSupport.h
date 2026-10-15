#pragma once

// Shared helpers of the P4-13 item model tests: static data bound from XML text (kept for the process like the DataManager holders keep
// templates) and the item model's static data lookups (model/items/detail/StaticDataLookups.h), installed for the tests so that they need no
// published DataManager holders; each double follows the contract of the Java lookup it stands for.

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/items/detail/StaticDataLookups.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/bonuses/StatBonusType.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"
#include "aion/gameserver/model/templates/stats/ModifiersTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::items::test {

/** Test registry behind the installed lookups (tests run on one thread; the registry is filled before the lookups are read) */
struct StaticDataRegistry {
	std::unordered_map<int32_t, const templates::item::ItemTemplate*> itemTemplates;
	/** Java ItemRandomBonusData: the modifier list of each (type, statBonusSetId); statBonusId is a 1-based index into the list */
	std::map<std::pair<templates::item::bonuses::StatBonusType, int32_t>, std::vector<const templates::stats::ModifiersTemplate*>> randomBonusSets;
	std::unordered_map<const templates::item::actions::ItemActions*, const templates::item::actions::PolishAction*> polishActions;
	std::unordered_map<const templates::item::actions::ItemActions*, const templates::item::actions::CraftLearnAction*> craftLearnActions;
	std::unordered_map<const templates::item::ItemTemplate*, const detail::TemperingTemplates*> temperingTemplates;
	std::unordered_map<int32_t, const templates::recipe::RecipeTemplate*> recipes;
	std::unordered_map<const templates::recipe::RecipeTemplate*, int32_t> recipeSkillIds;

	static StaticDataRegistry& get() {
		static StaticDataRegistry registry;
		return registry;
	}
};

/** Installs lookups over StaticDataRegistry for the scope; the destructor clears the registry and removes the lookups. */
class StaticDataScope {
public:
	StaticDataScope() {
		detail::StaticDataLookupsForTests lookups;
		lookups.itemTemplate = [](int32_t itemId) -> const templates::item::ItemTemplate* {
			auto& map = StaticDataRegistry::get().itemTemplates;
			auto it = map.find(itemId);
			return it == map.end() ? nullptr : it->second;
		};
		lookups.randomBonusTemplate = [](templates::item::bonuses::StatBonusType type, int32_t setId,
									   int32_t bonusId) -> const templates::stats::ModifiersTemplate* {
			// Java ItemRandomBonusData.getTemplate: null for a missing set, List.get(statBonusId - 1) otherwise
			auto& sets = StaticDataRegistry::get().randomBonusSets;
			auto it = sets.find({type, setId});
			if (it == sets.end())
				return nullptr;
			const int64_t index = static_cast<int64_t>(bonusId) - 1;
			if (index < 0 || index >= static_cast<int64_t>(it->second.size()))
				throw runtime::IndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(it->second.size()));
			return it->second[static_cast<size_t>(index)];
		};
		lookups.polishAction = [](const templates::item::actions::ItemActions& actions) -> const templates::item::actions::PolishAction* {
			auto& map = StaticDataRegistry::get().polishActions;
			auto it = map.find(&actions);
			return it == map.end() ? nullptr : it->second;
		};
		lookups.craftLearnAction = [](const templates::item::actions::ItemActions& actions) -> const templates::item::actions::CraftLearnAction* {
			auto& map = StaticDataRegistry::get().craftLearnActions;
			auto it = map.find(&actions);
			return it == map.end() ? nullptr : it->second;
		};
		lookups.temperingTemplates = [](const templates::item::ItemTemplate* itemTemplate) -> const detail::TemperingTemplates* {
			auto& map = StaticDataRegistry::get().temperingTemplates;
			auto it = map.find(itemTemplate);
			return it == map.end() ? nullptr : it->second;
		};
		lookups.recipeTemplateById = [](int32_t id) -> const templates::recipe::RecipeTemplate* {
			auto& map = StaticDataRegistry::get().recipes;
			auto it = map.find(id);
			return it == map.end() ? nullptr : it->second;
		};
		lookups.recipeSkillId = [](const templates::recipe::RecipeTemplate& recipe) -> int32_t {
			return StaticDataRegistry::get().recipeSkillIds.at(&recipe);
		};
		detail::setStaticDataLookupsForTests(lookups);
	}

	~StaticDataScope() {
		detail::setStaticDataLookupsForTests({});
		StaticDataRegistry::get() = StaticDataRegistry();
	}

	StaticDataScope(const StaticDataScope&) = delete;
	StaticDataScope& operator=(const StaticDataScope&) = delete;
};

/** Binds XML text as T and keeps the object for the process (static data is immortal). */
template <class T>
const T* bindStatic(std::string_view xml) {
	xml::LoadContext context;
	return xml::bindString<T>(context, xml).release();
}

/** Binds `<item_template attributes>children</item_template>` and registers it for the ITEM_DATA lookup under its id */
inline const templates::item::ItemTemplate* registerItem(std::string_view attributes, std::string_view children = {}) {
	const templates::item::ItemTemplate* itemTemplate =
		bindStatic<templates::item::ItemTemplate>("<item_template " + std::string(attributes) + ">" + std::string(children) + "</item_template>");
	StaticDataRegistry::get().itemTemplates[itemTemplate->getTemplateId()] = itemTemplate;
	return itemTemplate;
}

} // namespace aion::gameserver::model::items::test
