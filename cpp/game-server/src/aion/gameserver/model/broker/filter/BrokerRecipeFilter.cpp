#include "aion/gameserver/model/broker/filter/BrokerRecipeFilter.h"

#include "aion/gameserver/model/items/detail/StaticDataLookups.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/CraftLearnAction.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::broker::filter {

BrokerRecipeFilter::BrokerRecipeFilter(int32_t craftSkillIdValue, std::initializer_list<int32_t> masks)
	: BrokerContainsFilter(masks), craftSkillId(craftSkillIdValue) {
}

BrokerRecipeFilter::~BrokerRecipeFilter() = default;

runtime::Ref<BrokerRecipeFilter> BrokerRecipeFilter::create(int32_t craftSkillIdValue, std::initializer_list<int32_t> masks) {
	return runtime::makeRef<BrokerRecipeFilter>(craftSkillIdValue, masks);
}

bool BrokerRecipeFilter::accept(const templates::item::ItemTemplate* template_) {
	if (template_ == nullptr)
		throw runtime::NullPointerException("template");
	const templates::item::actions::CraftLearnAction* craftAction =
		template_->getActions() == nullptr ? nullptr : items::detail::getCraftLearnAction(*template_->getActions());
	if (craftAction == nullptr)
		return false;
	if (!BrokerContainsFilter::accept(template_))
		return false;
	int32_t id = craftAction->getRecipeId();
	const templates::recipe::RecipeTemplate* recipeTemplate = items::detail::getRecipeTemplateById(id);
	return recipeTemplate != nullptr && items::detail::getRecipeSkillId(*recipeTemplate) == craftSkillId;
}

} // namespace aion::gameserver::model::broker::filter
