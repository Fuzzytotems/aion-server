#include "aion/gameserver/services/toypet/PetService.h"

#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dao/PlayerPetsDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PetDopingData.h"
#include "aion/gameserver/dataholders/PetFeedData.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/PetSpecialFunction.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/common/legacy/LootGroupRules.h"
#include "aion/gameserver/model/team/common/legacy/LootRuleType.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/ItemUseLimits.h"
#include "aion/gameserver/model/templates/item/actions/AbstractItemAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/SkillUseAction.h"
#include "aion/gameserver/model/templates/pet/PetDopingBag.h"
#include "aion/gameserver/model/templates/pet/PetDopingEntry.h"
#include "aion/gameserver/model/templates/pet/PetFeedResult.h"
#include "aion/gameserver/model/templates/pet/PetFlavour.h"
#include "aion/gameserver/model/templates/pet/PetFunction.h"
#include "aion/gameserver/model/templates/pet/PetFunctionType.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/model/trade/TradeList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PET.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/services/TradeService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/services/toypet/PetFeedProgress.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/Util.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"

namespace aion::gameserver::services::toypet {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.toypet.PetService@L154:46 - useDoping's re-check after a teleport, pinned on the player
//   com.aionemu.gameserver.services.toypet.PetService@L161:46 - useDoping's re-check while the item may not be used, pinned on the player
//   com.aionemu.gameserver.services.toypet.PetService@L168:46 - useDoping's re-check after the item's cooldown, pinned on the player
//   com.aionemu.gameserver.services.toypet.PetService@L83:44 - the 2.5 s feeding step, pinned on the pet, the player and the item

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.toypet.PetService");

using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_PET;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

PetService& PetService::getInstance() {
	static PetService instance; // Java SingletonHolder
	return instance;
}

PetService::PetService() = default;

// Java PetService.java:53-61
void PetService::renamePet(model::gameobjects::player::Player& player, std::string_view name) {
	const std::string convertedName = utils::Util::convertName(name);
	runtime::Ptr<model::gameobjects::Pet> pet = player.getPet();
	if (pet != nullptr) {
		pet->getCommonData()->setName(convertedName);
		dao::PlayerPetsDAO::updatePetName(*pet->getCommonData());
		PacketSendUtility::broadcastPacket(player, SM_PET(pet->getObjectId(), pet->getName()), true);
	}
}

// Java PetService.java:63-67
void PetService::onPlayerLogin(model::gameobjects::player::Player& player) {
	std::vector<runtime::Ptr<model::gameobjects::player::PetCommonData>> playerPets = player.getPetList().getPets();
	if (!playerPets.empty())
		PacketSendUtility::sendPacket(player, SM_PET(playerPets));
}

// Java PetService.java:69-80
void PetService::removeObject(int32_t objectId, int32_t count, model::gameobjects::player::Player& player) {
	runtime::Ptr<model::gameobjects::Item> item = player.getInventory().getItemByObjId(objectId);
	if (item == nullptr || player.getPet() == nullptr || count > item->getItemCount())
		return;

	runtime::Ptr<model::gameobjects::Pet> pet = player.getPet();
	pet->getCommonData()->setCancelFeed(false);
	PacketSendUtility::sendPacket(player, SM_PET(1, item->getObjectId(), count, *pet));
	PacketSendUtility::sendPacket(player, SM_EMOTION(player, model::EmotionType::START_FEEDING, 0, player.getObjectId()));

	schedule(*pet, player, *item, count);
}

// Java PetService.java:82-87
void PetService::schedule(model::gameobjects::Pet& pet, model::gameobjects::player::Player& player, model::gameobjects::Item& item, int32_t count) {
	utils::ThreadPoolManager::getInstance().schedule(
		{this, &pet, &player, &item},
		[this, &pet, &player, &item, count] {
			if (!pet.getCommonData()->getCancelFeed())
				checkFeeding(pet, player, item, count);
		},
		2500);
}

// Java PetService.java:89-137
void PetService::checkFeeding(model::gameobjects::Pet& pet, model::gameobjects::player::Player& player, model::gameobjects::Item& item,
	int32_t count) {
	runtime::Ptr<model::gameobjects::player::PetCommonData> commonData = pet.getCommonData();
	runtime::Ptr<PetFeedProgress> progress = commonData->getFeedProgress();

	if (!commonData->getCancelFeed()) {
		const model::templates::pet::PetFunction* func = pet.getObjectTemplate()->getPetFunction(model::templates::pet::PetFunctionType::FOOD);
		if (func == nullptr) // Java: func.getId() on null
			throw runtime::NullPointerException("PetTemplate.getPetFunction(FOOD)");
		const model::templates::pet::PetFlavour* flavour = dataholders::DataManager::PET_FEED_DATA->getFlavourById(func->getId());
		if (flavour == nullptr) // Java: flavour.getFoodType on null
			throw runtime::NullPointerException("PetFeedData.getFlavourById(" + std::to_string(func->getId()) + ")");
		std::optional<model::templates::pet::FoodType> foodType = flavour->getFoodType(item.getItemId());

		// Java: isLovedFood(foodType, ...) with a null foodType answers false (findRewardGroup finds no group), progress is dereferenced only then
		if (foodType && flavour->isLovedFood(*foodType, item.getItemId())) {
			if (progress == nullptr)
				throw runtime::NullPointerException("PetCommonData.getFeedProgress()");
			if (progress->getLovedFoodRemaining() == 0)
				foodType = std::nullopt;
		}

		if (!foodType) {
			// non eatable item
			item::ItemPacketService::sendItemUnlockPacket(player, item);
			PacketSendUtility::sendPacket(player, SM_PET(5, 0, 0, pet));
			PacketSendUtility::sendPacket(player, SM_EMOTION(player, model::EmotionType::END_FEEDING, 0, player.getObjectId()));
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_TOYPET_FEED_FOOD_NOT_LOVEFLAVOR(pet.getName(), item.getItemTemplate()->getL10n()));
			return;
		}
		player.getInventory().decreaseItemCount(item, 1, item::ItemPacketService::ItemUpdateType::DEC_PET_FOOD);
		if (progress == nullptr) // Java: processFeedResult(progress, ...) dereferences it
			throw runtime::NullPointerException("PetCommonData.getFeedProgress()");
		const model::templates::pet::PetFeedResult* reward =
			flavour->processFeedResult(*progress, *foodType, item.getItemTemplate()->getLevel(), player.getCommonData()->getLevel());

		if (progress->getHungryLevel() == PetHungryLevel::FULL && reward != nullptr) {
			PacketSendUtility::sendPacket(player, SM_PET(2, item.getObjectId(), 0, pet));
			PacketSendUtility::sendPacket(player, SM_PET(6, reward->getItem(), 0, pet));
			PacketSendUtility::sendPacket(player, SM_PET(5, 0, 0, pet));
			PacketSendUtility::sendPacket(player, SM_EMOTION(player, model::EmotionType::END_FEEDING, 0, player.getObjectId()));
			PacketSendUtility::sendPacket(player, SM_PET(7, 0, 0, pet)); // 2151591961

			item::ItemService::addItem(player, reward->getItem(), 1);
			const int64_t delay = static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(flavour->getCooldDown()) * 60000u)); // Java int * int
			commonData->scheduleRefeed(delay);
			const int64_t refeedTime = commons::utils::currentTimeMillis() + delay;
			commonData->setRefeedTime(refeedTime);
			dao::PlayerPetsDAO::setTime(pet.getObjectId(), refeedTime);
			progress->reset();
		} else {
			PacketSendUtility::sendPacket(player, SM_PET(2, item.getObjectId(), --count, pet));
			if (count > 0)
				schedule(pet, player, item, count);
			else {
				PacketSendUtility::sendPacket(player, SM_PET(5, 0, 0, pet));
				PacketSendUtility::sendPacket(player, SM_EMOTION(player, model::EmotionType::END_FEEDING, 0, player.getObjectId()));
			}
		}
	}
}

// Java PetService.java:139-187
void PetService::useDoping(model::gameobjects::Pet& pet, int32_t action, int32_t itemId, int32_t slot, int32_t slot2) {
	if (pet.getCommonData()->getDopingBag() == nullptr)
		return;

	runtime::Ptr<model::gameobjects::player::Player> playerPtr = pet.getMaster();
	model::gameobjects::player::Player& player = *playerPtr;
	if (action < 2) { // add, replace or delete item
		if (!validateSetDopeItem(pet, itemId, slot))
			return;
		pet.getCommonData()->getDopingBag()->setItem(itemId, slot);
		PacketSendUtility::sendPacket(player, SM_PET(action, itemId, slot));
	} else if (action == 2) {
		pet.getCommonData()->getDopingBag()->switchItems(slot, slot2);
		PacketSendUtility::sendPacket(*pet.getMaster(), SM_PET(action, slot2, slot));
	} else if (action == 3) { // use item
		if (!player.isSpawned()) { // player may be just despawned because of a pending teleport, schedule re-check
			utils::ThreadPoolManager::getInstance().schedule(
				{&player}, [&player, action, itemId, slot] { PacketSendUtility::sendPacket(player, SM_PET(action, itemId, slot)); }, 5,
				runtime::TimeUnit::SECONDS);
			return;
		}

		std::vector<runtime::Ptr<model::gameobjects::Item>> items = player.getInventory().getItemsByItemId(itemId);
		if (items.empty()) // Java: List.get(0) on an empty list
			throw runtime::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
		model::gameobjects::Item& useItem = *items[0];

		if (!isPetItemUseAllowed(player, useItem)) { // pet currently is not allowed to buff, schedule re-check
			utils::ThreadPoolManager::getInstance().schedule(
				{&player}, [&player, action, itemId, slot] { PacketSendUtility::sendPacket(player, SM_PET(action, itemId, slot)); }, 20,
				runtime::TimeUnit::SECONDS);
			return;
		}

		const model::templates::item::ItemUseLimits* useLimits = useItem.getItemTemplate()->getUseLimits();
		if (useLimits == nullptr) // Java: getUseLimits().getDelayId() on null
			throw runtime::NullPointerException("ItemTemplate.getUseLimits()");
		const int64_t now = commons::utils::currentTimeMillis();
		const int64_t reuseTime = player.getItemReuseTime(useLimits->getDelayId());
		if (reuseTime != 0 && reuseTime > now) { // player still has cooldown, schedule re-check
			utils::ThreadPoolManager::getInstance().schedule(
				{&player}, [&player, action, itemId, slot] { PacketSendUtility::sendPacket(player, SM_PET(action, itemId, slot)); }, reuseTime - now);
			return;
		}

		const model::templates::item::actions::ItemActions* actions = useItem.getItemTemplate()->getActions();
		if (actions == nullptr) // Java: getActions().getItemActions() on null
			throw runtime::NullPointerException("ItemTemplate.getActions()");
		for (const std::unique_ptr<model::templates::item::actions::AbstractItemAction>& itemAction : actions->getItemActions()) {
			if (const auto* skillUse = dynamic_cast<const model::templates::item::actions::SkillUseAction*>(itemAction.get())) {
				PacketSendUtility::broadcastPacket(player,
					network::aion::serverpackets::SM_ITEM_USAGE_ANIMATION(player.getObjectId(), player.getObjectId(), useItem.getObjectId(),
						useItem.getItemId(), 0, 1, 1, 1, 0, 15360),
					true);
				skillengine::SkillEngine::getInstance().applyEffectDirectly(skillUse->getSkillId(), skillUse->getLevel(), player, player, std::nullopt,
					skillengine::model::Effect_ForceType::DEFAULT);
				const int32_t useDelay = useLimits->getDelayTime();
				player.addItemCoolDown(useLimits->getDelayId(), now + useDelay, useDelay / 1000);
				player.getInventory().decreaseByItemId(itemId, 1);
			} else
				log.warn("Pet attempt to use not skill use item");
		}
		PacketSendUtility::sendPacket(player, SM_PET(action, itemId, slot));
	}
}

// Java PetService.java:189-209
bool PetService::validateSetDopeItem(model::gameobjects::Pet& pet, int32_t itemId, int32_t slot) {
	const model::templates::pet::PetFunction* petFunction = pet.getObjectTemplate()->getPetFunction(model::templates::pet::PetFunctionType::DOPING);
	if (petFunction == nullptr) {
		utils::audit::AuditLogger::log(pet.getMaster(), "tried to set buff item " + std::to_string(itemId) + " but " + pet.toString() +
		                                                    " doesn't support buffing");
		return false;
	}
	const model::templates::pet::PetDopingEntry* dope = dataholders::DataManager::PET_DOPING_DATA->getDopingTemplate(petFunction->getId());
	if (dope == nullptr) // Java: dope.isUseFood() on null
		throw runtime::NullPointerException("PetDopingData.getDopingTemplate(" + std::to_string(petFunction->getId()) + ")");
	if (slot == 0 && !dope->isUseFood()) {
		utils::audit::AuditLogger::log(pet.getMaster(), "tried to set item " + std::to_string(itemId) + " in pet buff food slot but " + pet.toString() +
		                                                    " doesn't support buffing with food");
		return false;
	}
	if (slot == 1 && !dope->isUseDrink()) {
		utils::audit::AuditLogger::log(pet.getMaster(), "tried to set item " + std::to_string(itemId) + " in pet buff drink slot but " +
		                                                    pet.toString() + " doesn't support buffing with drinks");
		return false;
	}
	if (slot > 1 && slot - 1 > dope->getScrollsUsed()) {
		utils::audit::AuditLogger::log(pet.getMaster(), "tried to set item " + std::to_string(itemId) + " in pet buff scroll slot " +
		                                                    std::to_string(slot - 1) + " but " + pet.toString() + " only supports " +
		                                                    std::to_string(dope->getScrollsUsed()) + " scrolls");
		return false;
	}
	return true;
}

// Java PetService.java:211-219
bool PetService::isPetItemUseAllowed(model::gameobjects::player::Player& player, model::gameobjects::Item& item) {
	if (item.getItemTemplate()->hasAreaRestriction()) {
		const world::zone::ZoneName* restriction = item.getItemTemplate()->getUseArea();
		if (restriction != nullptr && !player.isInsideItemUseZone(restriction)) {
			return false;
		}
	}
	return true;
}

// Java PetService.java:222-237
void PetService::activateLoot(model::gameobjects::Pet& pet, bool activate) {
	if (activate) {
		if (!pet.getObjectTemplate()->containsFunction(model::templates::pet::PetFunctionType::LOOT)) {
			utils::audit::AuditLogger::log(pet.getMaster(), "tried to enable auto-loot on non-looting " + pet.toString());
			return;
		}
		runtime::Ptr<model::team::TemporaryPlayerTeam> team = pet.getMaster()->getCurrentTeam();
		if (team != nullptr && team->getLootGroupRules()->getLootRule() == model::team::common::legacy::LootRuleType::FREEFORALL) {
			PacketSendUtility::sendPacket(*pet.getMaster(), SM_SYSTEM_MESSAGE::STR_MSG_LOOTING_PET_MESSAGE03());
			return;
		}
		PacketSendUtility::sendPacket(*pet.getMaster(), SM_SYSTEM_MESSAGE::STR_MSG_LOOTING_PET_MESSAGE01());
	}
	pet.getCommonData()->setIsLooting(activate);
	PacketSendUtility::sendPacket(*pet.getMaster(), SM_PET(model::gameobjects::PetSpecialFunction::AUTOLOOT, activate));
}

// Java PetService.java:239-246
void PetService::activateAutoSell(model::gameobjects::Pet& pet, bool activate) {
	if (activate && !pet.getObjectTemplate()->containsFunction(model::templates::pet::PetFunctionType::MERCHANT)) {
		utils::audit::AuditLogger::log(pet.getMaster(), "tried to enable auto-sell on non-selling " + pet.toString());
		return;
	}
	pet.getCommonData()->setIsSelling(activate);
	PacketSendUtility::sendPacket(*pet.getMaster(), SM_PET(model::gameobjects::PetSpecialFunction::AUTOSELL, activate));
}

// Java PetService.java:248-261
void PetService::sell(runtime::Ptr<model::gameobjects::Pet> pet, const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	if (pet == nullptr || !pet->getCommonData()->isSelling())
		return;
	const model::templates::pet::PetFunction* pf = pet->getObjectTemplate()->getPetFunction(model::templates::pet::PetFunctionType::MERCHANT);
	if (pf != nullptr) {
		runtime::Ref<model::trade::TradeList> tradeList = model::trade::TradeList::create(pet->getObjectId());
		for (const runtime::Ptr<model::gameobjects::Item>& item : items)
			tradeList->addItem(item->getObjectId(), item->getItemCount());
		if (tradeList->size() > 0) {
			TradeService::performSellToShop(*pet->getMaster(), *tradeList, nullptr, pf->getRatePrice());
			PacketSendUtility::sendPacket(*pet->getMaster(), SM_SYSTEM_MESSAGE::STR_MSG_MERCHANT_PET_GET_SELL_ITEM(pet->getName()));
		}
	}
}

} // namespace aion::gameserver::services::toypet
