#include "aion/gameserver/services/player/PlayerService.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/configs/main/NameConfig.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/dao/AccountPassportsDAO.h"
#include "aion/gameserver/dao/BlockListDAO.h"
#include "aion/gameserver/dao/CraftCooldownsDAO.h"
#include "aion/gameserver/dao/FriendListDAO.h"
#include "aion/gameserver/dao/HeadhuntingDAO.h"
#include "aion/gameserver/dao/HouseObjectCooldownsDAO.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemCooldownsDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dao/MailDAO.h"
#include "aion/gameserver/dao/MotionDAO.h"
#include "aion/gameserver/dao/OldNamesDAO.h"
#include "aion/gameserver/dao/PlayerAppearanceDAO.h"
#include "aion/gameserver/dao/PlayerBindPointDAO.h"
#include "aion/gameserver/dao/PlayerCooldownsDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dao/PlayerEffectsDAO.h"
#include "aion/gameserver/dao/PlayerEmotionListDAO.h"
#include "aion/gameserver/dao/PlayerLifeStatsDAO.h"
#include "aion/gameserver/dao/PlayerMacrosDAO.h"
#include "aion/gameserver/dao/PlayerNpcFactionsDAO.h"
#include "aion/gameserver/dao/PlayerPunishmentsDAO.h"
#include "aion/gameserver/dao/PlayerQuestListDAO.h"
#include "aion/gameserver/dao/PlayerRecipesDAO.h"
#include "aion/gameserver/dao/PlayerSettingsDAO.h"
#include "aion/gameserver/dao/PlayerSkillListDAO.h"
#include "aion/gameserver/dao/PlayerTitleListDAO.h"
#include "aion/gameserver/dao/PortalCooldownsDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/functions/PlayerStatFunctions.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/PunishmentService_PunishmentType.h"
#include "aion/gameserver/services/SkillLearnService.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

namespace aion::gameserver::services::player {

using model::account::Account;
using model::account::PlayerAccountData;
using model::gameobjects::player::Player;
using model::gameobjects::player::PlayerCommonData;
using runtime::Ptr;
using runtime::Ref;

namespace {

// lint: L14 C++-only test seams (function pointers without state), written by tests before the flows they drive
std::atomic<PlayerService::PlayerFactory> playerFactoryForTests{nullptr};
// lint: L14 C++-only test seam, see above
std::atomic<PlayerService::LoadHook> loadHookForTests{nullptr};

// lint: L14 C++-only test seam, see above
std::atomic<PlayerService::StoreNewPlayerHook> storeNewPlayerHookForTests{nullptr};

/** Java: new Player(playerAccountData, account) */
Ref<Player> createPlayer(PlayerAccountData& playerAccountData, Account& account) {
	if (PlayerService::PlayerFactory factory = playerFactoryForTests.load(std::memory_order_acquire))
		return factory(playerAccountData, account);
	return model::gameobjects::VisibleObject::create<Player>(playerAccountData, account);
}

} // namespace

runtime::Ptr<Player> PlayerService::accountWarehouseActor(Account& account) {
	// the account warehouse is always a PlayerStorage (AccountService.java:96: new PlayerStorage(null, ACCOUNT_WAREHOUSE))
	auto* warehouse = dynamic_cast<model::items::storage::PlayerStorage*>(&account.getAccountWarehouse());
	return warehouse == nullptr ? nullptr : warehouse->getActor();
}

void PlayerService::restoreAccountWarehouseActor(Account& account, runtime::Ptr<Player> previousActor, Player& droppedPlayer) noexcept {
	try {
		if (previousActor && previousActor.get() != &droppedPlayer)
			account.getAccountWarehouse().setOwner(previousActor);
	} catch (...) {
		// the account has no warehouse (Java null): nothing to restore
	}
}

void PlayerService::setStoreNewPlayerHookForTests(StoreNewPlayerHook hook) noexcept {
	storeNewPlayerHookForTests.store(hook, std::memory_order_release);
}

void PlayerService::setPlayerFactoryForTests(PlayerFactory factory) noexcept {
	playerFactoryForTests.store(factory, std::memory_order_release);
}

void PlayerService::setLoadHookForTests(LoadHook hook) noexcept {
	loadHookForTests.store(hook, std::memory_order_release);
}

bool PlayerService::isNameUsedOrReserved(std::optional<std::string_view> oldName, std::string_view newName) {
	return isNameUsedOrReserved(oldName, newName, configs::main::NameConfig::RESERVE_OLD_NAME_DAYS.load());
}

bool PlayerService::isNameUsedOrReserved(std::optional<std::string_view> oldName, std::string_view newName, int32_t nameReservationDurationDays) {
	return dao::PlayerDAO::isNameUsed(newName) || dao::OldNamesDAO::isNameReserved(oldName, newName, nameReservationDurationDays);
}

bool PlayerService::storeNewPlayer(Player& player, std::string_view accountName, int32_t accountId) {
	if (!dao::PlayerDAO::saveNewPlayer(player, accountId, accountName))
		return false;
	// C++-only test seam between the two DAO calls (lint: L14): a false injects exactly the state Java produces when PlayerAppearanceDAO.store
	// fails - the players row is written, nothing else is (Java is not transactional here, CM_CREATE_CHARACTER.java:66-68)
	if (StoreNewPlayerHook hook = storeNewPlayerHookForTests.load(std::memory_order_acquire); hook != nullptr && !hook(player))
		return false;
	return dao::PlayerAppearanceDAO::store(player) && dao::PlayerSkillListDAO::storeSkills(player) && dao::InventoryDAO::store(player);
}

void PlayerService::storePlayer(Player& player) {
	using PunishmentType = services::PunishmentService_PunishmentType;
	dao::PlayerDAO::storePlayer(player);
	dao::PlayerSkillListDAO::storeSkills(player);
	dao::PlayerSettingsDAO::saveSettings(player);
	dao::PlayerQuestListDAO::store(player);
	dao::AbyssRankDAO::storeAbyssRank(player);
	dao::PlayerPunishmentsDAO::storePlayerPunishment(player, PunishmentType::PRISON);
	dao::PlayerPunishmentsDAO::storePlayerPunishment(player, PunishmentType::GATHER);
	dao::InventoryDAO::store(player);
	for (Ptr<model::house::House> house : *player.getHouses())
		house->save();
	dao::ItemStoneListDAO::save(player);
	dao::MailDAO::storeMailbox(player);
	dao::PortalCooldownsDAO::storePortalCooldowns(player);
	dao::CraftCooldownsDAO::storeCraftCooldowns(player);
	dao::HouseObjectCooldownsDAO::storeHouseObjectCooldowns(player);
	dao::PlayerNpcFactionsDAO::storeNpcFactions(player);
	dao::AccountPassportsDAO::storePassport(*player.getAccount());
	if (configs::main::EventsConfig::ENABLE_HEADHUNTING.load())
		dao::HeadhuntingDAO::storeHeadhunter(player.getObjectId());
}

Ref<Player> PlayerService::getPlayer(int32_t playerObjId, Ptr<Account> account) {
	// Player common data and appearance should be already loaded in account
	Ptr<PlayerAccountData> playerAccountData = account->getPlayerAccountData(playerObjId);
	Ptr<PlayerCommonData> pcd = playerAccountData->getPlayerCommonData();
	Ref<Player> player = createPlayer(*playerAccountData, *account);
	int32_t oldOwnerId = pcd->getWorldOwnerId();
	player->setPosition(world::World::getInstance().createPosition(pcd->getMapId(), pcd->getX(), pcd->getY(), pcd->getZ(), pcd->getHeading(), 0));
	pcd->setWorldOwnerId(oldOwnerId);
	Ptr<model::team::legion::LegionMember> legionMember = LegionService::getInstance().getLegionMember(*pcd);
	if (legionMember) {
		player->setLegionMember(legionMember);
	}

	player->setMacros(dao::PlayerMacrosDAO::loadMacros(playerObjId));
	player->setSkillList(dao::PlayerSkillListDAO::loadSkillList(playerObjId));
	player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*player));
	player->setFriendList(dao::FriendListDAO::load(*player));
	player->setBlockList(dao::BlockListDAO::load(playerObjId));
	player->setTitleList(dao::PlayerTitleListDAO::loadTitleList(playerObjId));
	player->setPlayerSettings(dao::PlayerSettingsDAO::loadSettings(playerObjId));
	dao::AbyssRankDAO::loadAbyssRank(*player);
	dao::PlayerNpcFactionsDAO::loadNpcFactions(*player);
	dao::MotionDAO::loadMotionList(*player);
	dao::AccountPassportsDAO::loadPassport(*player->getAccount());
	player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*player));
	player->setFlyController(std::make_unique<controllers::FlyController>(*player));
	model::stats::calc::functions::PlayerStatFunctions::addPredefinedStatFunctions(*player);

	player->setQuestStateList(dao::PlayerQuestListDAO::load(playerObjId));
	player->setRecipeList(dao::PlayerRecipesDAO::load(player->getObjectId()));

	// C++ only (m5a-plan.md S-05): the account warehouse is a part of the Account and shared by every Player created from it, so its actor may
	// already be another character of the same account that is online. Java leaves the actor at the failed player; the C++ breakers null it
	// (LogoutBreakers L3), which would turn the online character's account-warehouse operations into a null-actor dereference. Remember the
	// actor and put it back unless it was this player.
	runtime::Ptr<Player> previousWarehouseActor = accountWarehouseActor(*account);
	account->getAccountWarehouse().setOwner(player);
	// C++ only (m5a-plan.md S-05): from here on the account warehouse retains the Player (its actor). If a load below throws, the Player is
	// dropped by the caller, so the logout breakers cut the actor edge and the other C++-only edges (LogoutBreakers.h "Callers").
	auto breakers = runtime::finally([&player, &account, previousWarehouseActor]() noexcept {
		model::gameobjects::player::LogoutBreakers::run(*player);
		restoreAccountWarehouseActor(*account, previousWarehouseActor, *player);
	});
	if (LoadHook hook = loadHookForTests.load(std::memory_order_acquire))
		hook(*player);
	dao::InventoryDAO::loadStorage(playerObjId, player->getInventory());
	dao::ItemStoneListDAO::load(player->getInventory().getItems());
	dao::ItemStoneListDAO::load(player->getEquipment().getEquippedItemsWithoutStigma());

	dao::InventoryDAO::loadStorage(playerObjId, player->getWarehouse());
	dao::ItemStoneListDAO::load(player->getWarehouse().getItems());

	for (Ptr<model::items::storage::Storage> petBag : player->getPetBags()) {
		dao::InventoryDAO::loadStorage(playerObjId, *petBag);
		dao::ItemStoneListDAO::load(petBag->getItems());
	}
	for (Ptr<model::items::storage::Storage> cabinet : player->getCabinets()) {
		dao::InventoryDAO::loadStorage(playerObjId, *cabinet);
		dao::ItemStoneListDAO::load(cabinet->getItems());
	}

	// Apply equipment stats (items and manastones were loaded in account)
	player->getEquipment().onLoadApplyEquipmentStats();

	dao::PlayerPunishmentsDAO::loadPlayerPunishments(*player);

	// load saved effects
	dao::PlayerEffectsDAO::loadPlayerEffects(*player);
	// load saved player cooldowns
	dao::PlayerCooldownsDAO::loadPlayerCooldowns(*player);
	// load item cooldowns
	dao::ItemCooldownsDAO::loadItemCooldowns(*player);
	// load portal cooldowns
	dao::PortalCooldownsDAO::loadPortalCooldowns(*player);
	// load house object use cooldowns
	dao::HouseObjectCooldownsDAO::loadHouseObjectCooldowns(*player);
	// load bind point
	dao::PlayerBindPointDAO::loadBindPoint(*player);
	// load craft cooldowns
	dao::CraftCooldownsDAO::loadCraftCooldowns(*player);

	dao::PlayerLifeStatsDAO::loadPlayerLifeStat(*player);
	dao::PlayerEmotionListDAO::loadEmotions(*player);
	if (player->hasPermission(configs::main::MembershipConfig::EMOTIONS_ALL.load())) {
		// Java: for (int emotionId : EmotionLearnAction.getLearnableEmotionIds()) player.getEmotions().add(emotionId, 0, false);
		// TODO(header-request): request slice-1 asks P5-07 for EmotionLearnAction::getLearnableEmotionIds() (docs/deviations/P5-00.md; the
		// granted request player-7 covers only isLearnable(int32_t)); only accounts with the EMOTIONS_ALL membership (default 10) reach this
		// branch, so nothing on the M5a path is blocked
		AION_PARTIAL("EmotionLearnAction.getLearnableEmotionIds is not declared yet: EMOTIONS_ALL accounts get no extra emotions");
	}

	breakers.dismiss();
	return player;
}

Ref<Player> PlayerService::newPlayer(PlayerAccountData& playerAccountData, Account& account) {
	Ptr<PlayerCommonData> playerCommonData = playerAccountData.getPlayerCommonData();
	const dataholders::PlayerInitialData& playerInitialData = *dataholders::DataManager::PLAYER_INITIAL_DATA;
	const dataholders::PlayerInitialData::LocationData& ld = playerInitialData.getSpawnLocation(playerCommonData->getRace());

	playerCommonData->setMapId(ld.getMapId());
	playerCommonData->setX(ld.getX());
	playerCommonData->setY(ld.getY());
	playerCommonData->setZ(ld.getZ());
	playerCommonData->setHeading(ld.getHeading());

	Ref<Player> newPlayer = createPlayer(playerAccountData, account);

	// Starting skills
	newPlayer->setSkillList(model::skill::PlayerSkillList::create());
	SkillLearnService::learnNewSkills(*newPlayer, 1, newPlayer->getLevel());

	// Starting items
	const dataholders::PlayerInitialData::PlayerCreationData* playerCreationData =
		playerInitialData.getPlayerCreationData(playerCommonData->getPlayerClass());
	if (playerCreationData != nullptr) { // player transfer
		for (const dataholders::PlayerInitialData::PlayerCreationData::ItemType& itemType : playerCreationData->getItems()) {
			int32_t itemId = itemType.getTemplate()->getTemplateId();
			Ref<model::gameobjects::Item> item = services::item::ItemFactory::newItem(itemId, itemType.getCount());
			if (!item) {
				continue;
			}

			// When creating new player - all equipment that has slot values will be equipped
			// Make sure you will not put into xml file more items than possible to equip.
			const model::templates::item::ItemTemplate* itemTemplate = item->getItemTemplate();

			if ((itemTemplate->isArmor() || itemTemplate->isWeapon()) && !newPlayer->getEquipment().isSlotEquipped(itemTemplate->getItemSlot())) {
				item->setEquipped(true);
				model::items::ItemSlot itemSlot = model::items::getSlotFor(itemTemplate->getItemSlot());
				item->setEquipmentSlot(model::items::getSlotIdMask(itemSlot));
			}
			newPlayer->getInventory().onLoadHandler(*item);
		}
	}
	newPlayer->setMailbox(std::make_unique<model::gameobjects::player::Mailbox>(*newPlayer));

	// Mark inventory and equipment as UPDATE_REQUIRED to be saved during character creation
	newPlayer->getInventory().setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED);
	newPlayer->getEquipment().setPersistentState(model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED);
	return newPlayer;
}

Ref<PlayerCommonData> PlayerService::getOrLoadPlayerCommonData(int32_t playerObjId) {
	Ptr<Player> player = world::World::getInstance().getPlayer(playerObjId);
	if (!player)
		return dao::PlayerDAO::loadPlayerCommonData(playerObjId);
	return player->getCommonData();
}

Ref<PlayerCommonData> PlayerService::getOrLoadPlayerCommonData(std::string_view name) {
	Ptr<Player> player = world::World::getInstance().getPlayer(name);
	if (!player)
		return dao::PlayerDAO::loadPlayerCommonDataByName(name);
	return player->getCommonData();
}

bool PlayerService::cancelPlayerDeletion(PlayerAccountData& accData) {
	if (!accData.getDeletionDate()) {
		return true;
	}

	if (accData.getDeletionDate()->time_since_epoch().count() > commons::utils::currentTimeMillis()) {
		accData.setDeletionDate(std::nullopt);
		storeDeletionTime(accData);
		return true;
	}
	return false;
}

void PlayerService::deletePlayer(PlayerAccountData& accData) {
	if (accData.getDeletionDate()) {
		return;
	}

	// Java: new Timestamp(System.currentTimeMillis() + CHARACTER_DELETION_TIME_MINUTES * 60 * 1000) - int arithmetic in the minutes product
	const int32_t deletionMillis = configs::main::CustomConfig::CHARACTER_DELETION_TIME_MINUTES.load() * 60 * 1000;
	accData.setDeletionDate(commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis() + deletionMillis)));
	storeDeletionTime(accData);
}

void PlayerService::deletePlayerFromDB(int32_t playerId) {
	deletePlayerFromDB(playerId, true);
}

void PlayerService::deletePlayerFromDB(int32_t playerId, bool notifyServices) {
	dao::InventoryDAO::deletePlayerOrLegionItems(playerId);
	dao::PlayerDAO::deletePlayer(playerId);
	if (notifyServices) {
		HousingService::getInstance().onPlayerDeleted(playerId);
		BrokerService::getInstance().onPlayerDeleted(playerId);
	}
}

void PlayerService::storeDeletionTime(PlayerAccountData& accData) {
	dao::PlayerDAO::updateDeletionTime(accData.getPlayerCommonData()->getPlayerObjId(), accData.getDeletionDate());
}

void PlayerService::storeCreationTime(int32_t objectId, std::optional<commons::database::Timestamp> creationDate) {
	dao::PlayerDAO::storeCreationTime(objectId, creationDate);
}

void PlayerService::addMacro(Player& player, int32_t macroOrder, std::string_view macroXML) {
	AION_UNPORTED();
}

void PlayerService::removeMacro(Player& player, int32_t macroOrder) {
	AION_UNPORTED();
}

std::optional<std::string> PlayerService::getPlayerName(int32_t objectId) {
	Ptr<Player> player = world::World::getInstance().getPlayer(objectId);
	if (player)
		return player->getName();
	return dao::PlayerDAO::getPlayerNameByObjId(objectId);
}

} // namespace aion::gameserver::services::player
