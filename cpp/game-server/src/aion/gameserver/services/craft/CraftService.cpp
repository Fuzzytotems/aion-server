#include "aion/gameserver/services/craft/CraftService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/StaticObject.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Rates.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnumInfo.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/model/templates/item/ItemQuality.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CRAFT_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CRAFT_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/task/CraftingTask.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::services::craft {

static const auto log = commons::logging::LoggerFactory::getLogger("CRAFT_LOG");

namespace {

using model::gameobjects::player::Player;
using model::templates::item::ItemTemplate;
using model::templates::recipe::RecipeTemplate;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/**
 * Java's implicit null check of a dereference (`template.getX()` on a null lookup result): the pointer, or NullPointerException where the plain
 * C++ dereference would be undefined behaviour. `what` names the dereferenced expression.
 */
template <class T>
T* nonNull(T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return value;
}

/** Java unboxing of a nullable Integer (`int i = boxed`): the value, NullPointerException when absent */
int32_t unbox(const std::optional<int32_t>& value, const char* what) {
	if (!value)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/** Java int + int, int - int and int * int: two's complement wrap-around (signed overflow is undefined in C++) */
int32_t javaAdd(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

int32_t javaSub(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

int32_t javaMul(int32_t a, int32_t b) {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

/** Java (int) doubleValue: NaN is 0, out-of-range values saturate */
int32_t doubleToInt(double value) {
	if (std::isnan(value))
		return 0;
	if (value >= 2147483647.0)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

/** Java (int) floatValue: NaN is 0, out-of-range values saturate */
int32_t floatToInt(float value) {
	if (std::isnan(value))
		return 0;
	if (value >= 2147483648.0f)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0f)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

/** Java: DataManager.SKILL_DATA.getSkillTemplate(skillId).getL10n() (NullPointerException for an unknown skill) */
std::string skillL10n(int32_t skillId) {
	return nonNull(dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId), "SKILL_DATA.getSkillTemplate(skillId)")->getL10n();
}

/** Java: DataManager.ITEM_DATA.getItemTemplate(itemId).getL10n() (NullPointerException for an unknown item) */
std::string itemL10n(int32_t itemId) {
	return nonNull(dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId), "ITEM_DATA.getItemTemplate(itemId)")->getL10n();
}

/**
 * Java: componentsData.getComponent().get(0). ComponentsData binds its list by field (ComponentsData.java:13-22: XmlAccessType.FIELD, a plain
 * getter), and JAXB leaves the field null for a <components_data> without a <component>, so Java throws NullPointerException there; the bound
 * C++ list is empty instead. No shipped recipe has one (recipe_templates.xml: 0 of 12,494).
 */
const model::templates::recipe::Component& firstComponent(const model::templates::recipe::ComponentsData& componentsData) {
	if (componentsData.getComponent().empty())
		throw runtime::NullPointerException("componentsData.getComponent() is null");
	return componentsData.getComponent().front();
}

// fieldmap-class: com.aionemu.gameserver.services.craft.CraftService$1
/**
 * Java: the anonymous ItemUpdatePredicate of finishCrafting (CraftService.java:73-83, fieldmap key CraftService$1), passed to
 * ItemService.addItem and used during the call only (storage: sync): a crafted weapon or armor carries the crafter's name.
 */
class CraftService_ItemUpdatePredicate final : public item::ItemService::ItemUpdatePredicate {
	AION_MAKE_REF_FRIEND
public:
	const runtime::Ref<Player> player; // captured param Player player (line 43) [captured variable: final object reference]

	static runtime::Ref<CraftService_ItemUpdatePredicate> create(Player& playerValue) {
		return runtime::makeRef<CraftService_ItemUpdatePredicate>(playerValue);
	}

	// Java CraftService.java:75-82
	bool changeItem(model::gameobjects::Item& item) override {
		if (item.getItemTemplate()->isWeapon() || item.getItemTemplate()->isArmor()) {
			item.setItemCreator(player->getName());
			return true;
		}
		return false;
	}

protected:
	explicit CraftService_ItemUpdatePredicate(Player& playerValue)
		: ItemUpdatePredicate(item::ItemPacketService_ItemAddType::CRAFTED_ITEM, item::ItemPacketService_ItemUpdateType::INC_ITEM_COLLECT),
		  player(playerValue) {}
	~CraftService_ItemUpdatePredicate() override = default;
};

} // namespace

// Java CraftService.java:43-95; the item predicate is CraftService_ItemUpdatePredicate above (fieldmap key CraftService$1, storage: sync)
void CraftService::finishCrafting(model::gameobjects::player::Player& player, const model::templates::recipe::RecipeTemplate* recipetemplate, int32_t critCount, int32_t bonus) {
	const RecipeTemplate& recipe = *nonNull(recipetemplate, "recipetemplate");

	if (recipe.getMaxProductionCount()) {
		player.getRecipeList()->deleteRecipe(player, recipe.getId());
		if (critCount == 0) {
			runtime::Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(nullptr, player, 0);
			std::optional<int32_t> comboProduct = recipe.getComboProduct(1);
			questEngine::QuestEngine::getInstance().onFailCraft(*env, comboProduct ? *comboProduct : 0);
		}
	}

	int32_t skillId = recipe.getSkillId();
	int32_t skillLvl = recipe.getSkillpoint();
	// Java: (int) ((0.008 * (skillLvl + 100) * (skillLvl + 100) + 60)) - the sum in int, the rest in double
	int32_t xpReward = doubleToInt(0.008 * javaAdd(skillLvl, 100) * javaAdd(skillLvl, 100) + 60);
	xpReward = javaAdd(xpReward, javaMul(xpReward, bonus) / 100); // bonus (int division truncates towards zero in both languages)
	int32_t gainedCraftXp = model::gameobjects::player::calcResult(model::gameobjects::player::Rates::SKILL_XP_CRAFTING, player, xpReward);
	std::optional<model::stats::container::StatEnum> boostStat = model::stats::container::getModifier(skillId);
	if (boostStat) // there is no boost for morphing (40009); Java: gainedCraftXp *= current / 100f (int * float, narrowed back to int)
		gainedCraftXp = floatToInt(static_cast<float>(gainedCraftXp) *
			(static_cast<float>(player.getGameStats()->getStat(*boostStat, 100.0f)->getCurrent()) / 100.0f));
	gainedCraftXp = std::max(1, gainedCraftXp);

	if (player.getSkillList()->addSkillXp(player, skillId, gainedCraftXp, skillLvl)) {
		player.getCommonData()->addExp(xpReward, model::gameobjects::player::Rates::XP_CRAFTING);
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_DONT_GET_PRODUCTION_EXP(skillL10n(skillId)));
	}

	int32_t productItemId = critCount > 0 ? unbox(recipe.getComboProduct(critCount), "recipetemplate.getComboProduct(critCount)")
										  : recipe.getProductId();

	runtime::Ref<CraftService_ItemUpdatePredicate> predicate = CraftService_ItemUpdatePredicate::create(player);
	item::ItemService::addItem(player, productItemId, recipe.getQuantity(), true, *predicate);

	if (configs::main::LoggingConfig::LOG_CRAFT.load()) {
		const ItemTemplate* itemTemplate = nonNull(dataholders::DataManager::ITEM_DATA->getItemTemplate(productItemId), "itemTemplate");
		log.info("Player " + player.getName() + " crafted item " + std::to_string(productItemId) + " [" + itemTemplate->getName() + "] (count: " +
			std::to_string(recipe.getQuantity()) + ")" + (critCount > 0 ? " - critical" : ""));
	}

	if (recipe.getCraftDelayId()) {
		// Java: System.currentTimeMillis() + recipetemplate.getCraftDelayTime() * 1000 - the product in int, the sum in long
		int64_t reuseTimeMillis = commons::utils::currentTimeMillis() + javaMul(unbox(recipe.getCraftDelayTime(), "craftDelayTime"), 1000);
		player.getCraftCooldowns()->put(*recipe.getCraftDelayId(), reuseTimeMillis);
	}
}

// Java CraftService.java:97-132
void CraftService::startCrafting(model::gameobjects::player::Player& player, int32_t recipeId, int32_t targetObjId, int32_t craftType, const std::unordered_map<int32_t, int64_t>& sendMaterialsData) {
	const RecipeTemplate* recipeTemplate = dataholders::DataManager::RECIPE_DATA->getRecipeTemplateById(recipeId);
	int32_t skillId = nonNull(recipeTemplate, "recipeTemplate")->getSkillId();
	runtime::Ptr<model::gameobjects::VisibleObject> target = player.getKnownList().getObject(targetObjId);
	const ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(recipeTemplate->getProductId());

	if (!checkCraft(player, recipeTemplate, skillId, target, itemTemplate, craftType, sendMaterialsData)) {
		sendCancelCraft(player, skillId, targetObjId, itemTemplate);
		return;
	}

	// Java: getDp() != null - the boxed int is never null
	player.getCommonData()->addDp(javaSub(0, recipeTemplate->getDp()));

	int32_t intervalCap = 1200;
	std::optional<model::templates::item::ItemQuality> quality = itemTemplate->getItemQuality();
	if (!quality) // Java: switch on a null enum
		throw runtime::NullPointerException("itemTemplate.getItemQuality() is null");
	switch (*quality) {
		case model::templates::item::ItemQuality::UNIQUE:
		case model::templates::item::ItemQuality::EPIC:
			intervalCap = 1500;
			break;
		case model::templates::item::ItemQuality::MYTHIC:
			intervalCap = 1700;
			break;
		default:
			break;
	}
	int32_t skillLvlDiff = javaSub(player.getSkillList()->getSkillLevel(skillId), recipeTemplate->getSkillpoint());
	// Java: (StaticObject) target - null for a morph without a target (checkCraft skips the target check for 40009, CraftService.java:149-157),
	// ClassCastException for a known object that is not a StaticObject (an npc the morphing player targets); m5c-plan.md C-01
	runtime::Ptr<model::gameobjects::StaticObject> responder = runtime::cast<model::gameobjects::StaticObject>(target);
	runtime::Ref<skillengine::task::CraftingTask> craftingTask =
		skillengine::task::CraftingTask::create(player, responder, recipeTemplate, skillLvlDiff, craftType == 1 ? 15 : 0);

	if (skillId == 40009) {
		craftingTask->setInterval(200);
	} else {
		int32_t interval = javaSub(2500, javaMul(skillLvlDiff, 60));
		craftingTask->setInterval(interval < intervalCap ? intervalCap : interval);
	}
	craftingTask->start();
}

// Java CraftService.java:134-233
bool CraftService::checkCraft(model::gameobjects::player::Player& player, const model::templates::recipe::RecipeTemplate* recipeTemplate, int32_t skillId, runtime::Ptr<model::gameobjects::VisibleObject> target, const model::templates::item::ItemTemplate* itemTemplate, int32_t craftType, const std::unordered_map<int32_t, int64_t>& sendMaterialsData) {
	if (recipeTemplate == nullptr) {
		return false;
	}

	if (itemTemplate == nullptr) {
		return false;
	}

	if (runtime::Ptr<skillengine::task::CraftingTask> craftingTask = runtime::as<skillengine::task::CraftingTask>(player.getInteractionTask());
		craftingTask && craftingTask->isInProgress()) {
		return false;
	}

	// morphing dont need static object/npc to use
	if ((skillId != 40009)) {
		if (!target || !runtime::as<model::gameobjects::StaticObject>(target)) {
			utils::audit::AuditLogger::log(player, "tried to craft with incorrect target");
			return false;
		} else if (!utils::PositionUtil::isInRange(player, *target, 5, false)) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMBINE_TOO_FAR_FROM_TOOL(
				nonNull(target->getObjectTemplate(), "target.getObjectTemplate()")->getL10n()));
			return false;
		}
	}

	// Java: getDp() != null - the boxed int is never null
	if (player.getCommonData()->getDp() < recipeTemplate->getDp()) {
		utils::audit::AuditLogger::log(player, "tried to craft without required DP count");
		return false;
	}

	if (player.isInPlayerMode(model::actions::PlayerMode::RIDE) || player.isInAnyHide()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_COMBINE_WHILE_IN_CURRENT_STANCE());
		return false;
	}

	if (player.getInventory().isFull()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMBINE_INVENTORY_IS_FULL());
		return false;
	}

	if (!player.getRecipeList()->isRecipePresent(recipeTemplate->getId())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMBINE_CAN_NOT_FIND_RECIPE());
		return false;
	}

	if (recipeTemplate->getCraftDelayId() && player.getCraftCooldowns()->hasCooldown(*recipeTemplate->getCraftDelayId())) {
		// since there's no SM_CRAFT_COOLDOWN (at least we didn't find it yet), we must send some sys message to the player instead of audit logging
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ITEM_CANT_USE_UNTIL_DELAY_TIME());
		// AuditLogger.log(player, "tried to craft before cooldown expired");
		return false;
	}

	if (!player.getSkillList()->isSkillPresent(skillId)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMBINE_CANT_USE(skillL10n(skillId)));
		return false;
	}

	if (player.getSkillList()->getSkillLevel(skillId) < recipeTemplate->getSkillpoint()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMBINE_OUT_OF_SKILL_POINT(skillL10n(skillId)));
		return false;
	}

	for (const model::templates::recipe::ComponentsData& componentsData : recipeTemplate->getComponents()) {
		const model::templates::recipe::Component& first = firstComponent(componentsData);
		if (!sendMaterialsData.contains(first.getItemId()))
			continue;
		for (const model::templates::recipe::Component& component : componentsData.getComponent()) {
			int64_t availableComponentCount = player.getInventory().getItemCountByItemId(component.getItemId());
			if (availableComponentCount < component.getQuantity()) {
				std::string l10n = itemL10n(component.getItemId());
				if (component.getQuantity() == 1)
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMBINE_NO_COMPONENT_ITEM_SINGLE(l10n));
				else
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMBINE_NO_COMPONENT_ITEM_MULTIPLE(component.getQuantity(), l10n));
				return false;
			}
		}
		break;
	}

	if (craftType == 1 && !player.getInventory().decreaseByItemId(getBonusReqItem(skillId), 1)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_COMBINE_NO_COMPONENT_ITEM_SINGLE(itemL10n(getBonusReqItem(skillId))));
		return false;
	}

	for (const model::templates::recipe::ComponentsData& componentsData : recipeTemplate->getComponents()) {
		const model::templates::recipe::Component& first = firstComponent(componentsData);
		if (!sendMaterialsData.contains(first.getItemId()))
			continue;

		for (const model::templates::recipe::Component& component : componentsData.getComponent())
			player.getInventory().decreaseByItemId(component.getItemId(), component.getQuantity());
		break;
	}

	return true;
}

// Java CraftService.java:235-238
void CraftService::sendCancelCraft(model::gameobjects::player::Player& player, int32_t skillId, int32_t targetObjId, const model::templates::item::ItemTemplate* itemTemplate) {
	// Java: new SM_CRAFT_UPDATE(skillId, null, ...) throws NullPointerException at item.getTemplateId() (a recipe whose product has no template)
	PacketSendUtility::sendPacket(player,
		network::aion::serverpackets::SM_CRAFT_UPDATE(skillId, nonNull(itemTemplate, "itemTemplate"), 0, 0, 4, 0, 0));
	PacketSendUtility::broadcastPacket(player, network::aion::serverpackets::SM_CRAFT_ANIMATION(player.getObjectId(), targetObjId, 0, 2), true);
}

// Java CraftService.java:240-258
int32_t CraftService::getBonusReqItem(int32_t skillId) {
	switch (skillId) {
		case 40001: // Cooking
			return 169401081;
		case 40002: // Weaponsmithing
			return 169401076;
		case 40003: // Armorsmithing
			return 169401077;
		case 40004: // Tailoring
			return 169401078;
		case 40007: // Alchemy
			return 169401080;
		case 40008: // Handicrafting
			return 169401079;
		case 40010: // Menusier
			return 169401082;
		default:
			break;
	}
	return 0;
}

} // namespace aion::gameserver::services::craft
