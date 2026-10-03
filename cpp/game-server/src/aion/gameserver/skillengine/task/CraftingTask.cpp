#include "aion/gameserver/skillengine/task/CraftingTask.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "aion/commons/configuration/ConfigValue.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CRAFT_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CRAFT_UPDATE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/craft/CraftService.h"
#include "aion/gameserver/skillengine/task/AbstractCraftTask_CraftTypeInfo.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::skillengine::task {

using configs::main::RatesConfig;
using gameserver::model::templates::item::ItemQuality;
using gameserver::model::templates::item::ItemTemplate;
using gameserver::model::templates::recipe::RecipeTemplate;
using network::aion::serverpackets::SM_CRAFT_ANIMATION;
using network::aion::serverpackets::SM_CRAFT_UPDATE;
using utils::PacketSendUtility;

namespace {

/** Java: recipeTemplate.getComboProductSize() in the constructor - a null recipe is a NullPointerException there (CraftingTask.java:34) */
int32_t comboProductSizeOf(const RecipeTemplate* recipeTemplate) {
	if (!recipeTemplate)
		throw runtime::NullPointerException("Cannot invoke \"RecipeTemplate.getComboProductSize()\" because \"recipeTemplate\" is null");
	return recipeTemplate->getComboProductSize();
}

/**
 * Java hands `itemTemplate` to SM_CRAFT_UPDATE, whose constructor reads `item.getTemplateId()`, and switches on `itemTemplate.getItemQuality()`:
 * a template missing from the item data (DataManager.ITEM_DATA answers null) is a NullPointerException at that point. The C++ packet would read
 * a null pointer instead, so the throw is explicit. Every product and combo product of recipe_templates.xml has a template (measured: 12,494
 * recipes, none missing), so no shipped recipe gets here.
 */
const ItemTemplate* present(const ItemTemplate* itemTemplate) {
	if (!itemTemplate)
		throw runtime::NullPointerException("Cannot invoke \"ItemTemplate.getTemplateId()\" because \"item\" is null");
	return itemTemplate;
}

/** Java `switch (itemTemplate.getItemQuality())`: a template without a quality is a NullPointerException */
ItemQuality qualityOf(const ItemTemplate* itemTemplate) {
	std::optional<ItemQuality> quality = present(itemTemplate)->getItemQuality();
	if (!quality)
		throw runtime::NullPointerException("Cannot invoke \"ItemQuality.ordinal()\" because \"itemTemplate.getItemQuality()\" is null");
	return *quality;
}

/** Java Rates.get(player, RatesConfig.X) on a snapshot of the reloadable config value */
float rate(gameserver::model::gameobjects::player::Player& player,
	const commons::configuration::ConfigValue<std::vector<float>>& membershipRates) {
	std::shared_ptr<const std::vector<float>> rates = membershipRates.get();
	return gameserver::model::gameobjects::player::get(player, rates ? *rates : std::vector<float>());
}

/** Java `(int) value` of a float: NaN 0, out-of-range values saturate (a plain C++ cast is undefined there) */
int32_t javaFloatToInt(float value) {
	if (value != value)
		return 0;
	if (value >= 2147483648.0f)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0f)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

} // namespace

CraftingTask::CraftingTask(gameserver::model::gameobjects::player::Player& requesterValue,
	runtime::Ptr<gameserver::model::gameobjects::StaticObject> responderValue, const RecipeTemplate* recipeTemplateValue,
	int32_t skillLvlDiffValue, int32_t bonusValue)
	: AbstractCraftTask(requesterValue, responderValue, skillLvlDiffValue), recipeTemplate(recipeTemplateValue),
	  maxCritCount(comboProductSizeOf(recipeTemplateValue)), bonus(bonusValue) {
	itemTemplate.set(dataholders::DataManager::ITEM_DATA->getItemTemplate(recipeTemplate->getProductId()));
}

CraftingTask::~CraftingTask() = default;

runtime::Ref<CraftingTask> CraftingTask::create(gameserver::model::gameobjects::player::Player& requesterValue,
	runtime::Ptr<gameserver::model::gameobjects::StaticObject> responderValue,
	const gameserver::model::templates::recipe::RecipeTemplate* recipeTemplateValue, int32_t skillLvlDiffValue, int32_t bonusValue) {
	return runtime::makeRef<CraftingTask>(requesterValue, responderValue, recipeTemplateValue, skillLvlDiffValue, bonusValue);
}

void CraftingTask::onFailureFinish() {
	PacketSendUtility::sendPacket(*requester, SM_CRAFT_UPDATE(recipeTemplate->getSkillId(), present(itemTemplate.get()),
												currentSuccessValue.get(), currentFailureValue.get(), 6, 0, 0));
	PacketSendUtility::broadcastPacket(*requester, SM_CRAFT_ANIMATION(requester->getObjectId(), responder->getObjectId(), 0, 3), true);
}

bool CraftingTask::onSuccessFinish() {
	if (calculateCrit()) {
		onInteractionStart();
		return false;
	} else {
		PacketSendUtility::sendPacket(*requester, SM_CRAFT_UPDATE(recipeTemplate->getSkillId(), present(itemTemplate.get()),
													currentSuccessValue.get(), currentFailureValue.get(), 5, 0, 0));
		PacketSendUtility::broadcastPacket(*requester, SM_CRAFT_ANIMATION(requester->getObjectId(), responder->getObjectId(), 0, 2), true);
		services::craft::CraftService::finishCrafting(*requester, recipeTemplate, critCount.get(), bonus);
		return true;
	}
}

bool CraftingTask::calculateCrit() {
	if (critCount.get() >= maxCritCount)
		return false;

	// Java: getComboProduct(critCount + 1) == null. Unreachable with a bound recipe: below maxCritCount (the size of the combo product list) the
	// element exists, and its item id is a plain int (ComboProduct.java: `protected int itemid`), never null
	if (!recipeTemplate->getComboProduct(critCount.get() + 1))
		return false;

	// first crit uses base rate, subsequent crits use combo rate
	float chance;
	if (critCount.get() == 0)
		chance = rate(*requester, RatesConfig::CRAFT_CRIT_CHANCES);
	else
		chance = rate(*requester, RatesConfig::CRAFT_COMBO_CHANCES);
	runtime::Ptr<gameserver::model::house::House> house = requester->getActiveHouse();
	if (house)
		switch (house->getHouseType()) {
			case gameserver::model::templates::housing::HouseType::ESTATE:
			case gameserver::model::templates::housing::HouseType::PALACE:
				chance += 5;
				break;
			default:
				break;
		}

	if (commons::utils::Rnd::chance() >= chance)
		return false;

	critCount.set(critCount.get() + 1);
	itemTemplate.set(dataholders::DataManager::ITEM_DATA->getItemTemplate(*recipeTemplate->getComboProduct(critCount.get())));
	return true;
}

void CraftingTask::sendInteractionUpdate() {
	PacketSendUtility::sendPacket(*requester, SM_CRAFT_UPDATE(recipeTemplate->getSkillId(), present(itemTemplate.get()),
												currentSuccessValue.get(), currentFailureValue.get(), getProgressId(craftType.get()),
												executionSpeed.get(), showBarDelay.get()));
}

void CraftingTask::onInteractionAbort() {
	PacketSendUtility::sendPacket(*requester, SM_CRAFT_UPDATE(recipeTemplate->getSkillId(), present(itemTemplate.get()), 0, 0, 4, 0, 0));
	PacketSendUtility::broadcastPacket(*requester, SM_CRAFT_ANIMATION(requester->getObjectId(), responder->getObjectId(), 0, 2), true);
}

void CraftingTask::onInteractionFinish() {
}

void CraftingTask::onInteractionStart() {
	currentSuccessValue.set(0);
	currentFailureValue.set(0);

	PacketSendUtility::sendPacket(*requester, SM_CRAFT_UPDATE(recipeTemplate->getSkillId(), present(itemTemplate.get()), fullBarValue,
												fullBarValue, critCount.get() == 0 ? 0 : 3, 0, 0));
	PacketSendUtility::sendPacket(*requester, SM_CRAFT_UPDATE(recipeTemplate->getSkillId(), present(itemTemplate.get()), 0, 0, 1, 0, 0));
	PacketSendUtility::broadcastPacket(*requester,
		SM_CRAFT_ANIMATION(requester->getObjectId(), responder->getObjectId(), recipeTemplate->getSkillId(), 0), true);
	PacketSendUtility::broadcastPacket(*requester,
		SM_CRAFT_ANIMATION(requester->getObjectId(), responder->getObjectId(), recipeTemplate->getSkillId(), 1), true);
}

void CraftingTask::analyzeInteraction() {
	const int32_t diff = skillLvlDiff.get();
	if (recipeTemplate->getSkillId() == 40009) { // morph
		currentSuccessValue.set(fullBarValue);
		return;
	} else if (diff < 0) {
		currentFailureValue.set(fullBarValue);
		return;
	}

	craftType.set(CraftType::NORMAL);
	float multi = commons::utils::Rnd::nextFloat(1.0f, 2.0f);
	float failReduction = std::max(1.0f - static_cast<float>(diff) * 0.015f, 0.25f); // dynamic fail rate multiplier
	bool success = diff >= 41 ||
		commons::utils::Rnd::chance() >= static_cast<float>(configs::main::CraftConfig::MAX_CRAFT_FAILURE_CHANCE.load()) * failReduction;

	float bonusModifier = 1.0f;
	switch (qualityOf(itemTemplate.get())) {
		case ItemQuality::LEGEND:
			bonusModifier = 0.9f;
			break;
		case ItemQuality::UNIQUE:
			bonusModifier = 0.7f;
			break;
		case ItemQuality::EPIC:
			bonusModifier = 0.5f;
			break;
		case ItemQuality::MYTHIC:
			bonusModifier = 0.3f;
			break;
		default:
			break;
	}

	if (success) {
		if (commons::utils::Rnd::chance() < (15.0f + static_cast<float>(diff) / 3.0f))
			craftType.set(CraftType::CRIT_BLUE); // LIGHT BLUE + 10-20%

		int32_t minStep = 70;
		int32_t lvlBoni = diff > 10 ? ((diff - 10) * 2) : 0;
		int32_t stepBonus = javaFloatToInt((static_cast<float>(craftType.get() == CraftType::CRIT_BLUE ? 100 : 0) +
												((static_cast<float>(diff + 1) / 2.0f) + static_cast<float>(lvlBoni)) * 10.0f) *
			multi);
		currentSuccessValue.set(
			currentSuccessValue.get() + utils::JavaMath::round(static_cast<float>(minStep) + (static_cast<float>(stepBonus) * bonusModifier)));
	} else {
		int32_t minStep = recipeTemplate->getMaxProductionCount() ? 70 : 120;
		int32_t stepBonus = javaFloatToInt((static_cast<float>(diff + 1) / 1.5f * 10.0f) * multi);
		currentFailureValue.set(
			currentFailureValue.get() + utils::JavaMath::round(static_cast<float>(minStep) + (static_cast<float>(stepBonus) * bonusModifier)));
	}

	if (currentSuccessValue.get() > fullBarValue)
		currentSuccessValue.set(fullBarValue);
	else if (currentFailureValue.get() > fullBarValue)
		currentFailureValue.set(fullBarValue);

	int32_t speed = bonusModifier < 1.0f ? utils::JavaMath::round(900.0f * (2.0f - bonusModifier)) : (900 - (diff * 30));
	executionSpeed.set(std::max(speed, 300));
	showBarDelay.set(bonusModifier < 1.0f ? 1200 : std::max(500, 1200 - (diff * 30)));
}

} // namespace aion::gameserver::skillengine::task
