#include "aion/gameserver/services/player/PlayerEnterWorldService.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/cache/HTMLCache.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/configs/main/CraftConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/HTMLConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/configs/main/PeriodicSaveConfig.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/RVController.h"
#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/dao/BookmarkDAO.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dao/PlayerPasskeyDAO.h"
#include "aion/gameserver/dao/PlayerPunishmentsDAO.h"
#include "aion/gameserver/dao/PlayerQuestListDAO.h"
#include "aion/gameserver/dao/PlayerSkillListDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/custom/pvpmap/PvpMapService.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/Expirable.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"
#include "aion/gameserver/model/account/CharacterPasskey.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/Cooldowns.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/house/PlayerScript.h"
#include "aion/gameserver/model/items/ItemCooldown.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/model/gameobjects/player/title/Title.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HouseType.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AFTER_TIME_CHECK_4_7_5.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHARACTER_SELECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ENTER_WORLD_CHECK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GAME_TIME.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GM_BOOKMARK_ADD.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_SCRIPTS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_DOMINION_LOC_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MACRO_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PRICES.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECIPE_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TITLE_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UI_SETTINGS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UNK_3_5_1.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WAREHOUSE_INFO.h"
#include "aion/gameserver/network/aion/skillinfo/SkillEntryWriter.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/services/AccountService.h"
#include "aion/gameserver/services/AtreianPassportService.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/BonusPackService.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/services/ClassChangeService.h"
#include "aion/gameserver/services/FactionPackService.h"
#include "aion/gameserver/services/HTMLService.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/KiskService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/PunishmentService.h"
#include "aion/gameserver/services/PunishmentService_PunishmentType.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/StigmaService.h"
#include "aion/gameserver/services/SurveyService.h"
#include "aion/gameserver/services/VortexService.h"
#include "aion/gameserver/services/WarehouseService.h"
#include "aion/gameserver/services/abyss/AbyssSkillService.h"
#include "aion/gameserver/services/craft/RelinquishCraftStatus.h"
#include "aion/gameserver/services/event/EventService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/mail/MailService.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/player/MultiClientingService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/reward/AdventService.h"
#include "aion/gameserver/services/reward/VeteranRewardService.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/services/teleport/BindPointTeleportService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/services/toypet/PetService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/GMService.h"
#include "aion/gameserver/utils/collections/DynamicServerPacketBodySplitList.h"
#include "aion/gameserver/utils/collections/FixedElementCountSplitList.h"
#include "aion/gameserver/utils/collections/ListPart.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::player {

static const auto log = commons::logging::LoggerFactory::getLogger("GAMECONNECTION_LOG");

using model::account::Account;
using model::account::PlayerAccountData;
using model::gameobjects::player::Player;
using model::gameobjects::player::PlayerCommonData;
using network::aion::AionConnection;
using network::aion::serverpackets::SM_ENTER_WORLD_CHECK;
using runtime::Ptr;
using runtime::Ref;

/**
 * Java: package-private top-level class GeneralUpdateTask implements Runnable (PlayerEnterWorldService.java; used only by enterWorld). A task
 * object of scheduleAtFixedRate with only immutable members (fieldmap K3), ported as a TaskStruct value (runtime-architecture.md §7.3,
 * §14.2(f) GeneralUpdateTask).
 */
class GeneralUpdateTask : public runtime::TaskStruct {
public:
	const int32_t playerId;
	/** Java: run() */
	void operator()() const;
};

/** Java: package-private top-level class ItemUpdateTask implements Runnable (PlayerEnterWorldService.java; used only by enterWorld) */
class ItemUpdateTask : public runtime::TaskStruct {
public:
	const int32_t playerId;
	/** Java: run() */
	void operator()() const;
};

// Java: GeneralUpdateTask.log and ItemUpdateTask.log
static const auto generalUpdateTaskLog = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.player.GeneralUpdateTask");
static const auto itemUpdateTaskLog = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.player.ItemUpdateTask");

namespace {

/** Java: byte[] of a player settings field as the packet parameter */
std::vector<uint8_t> bytesOf(runtime::Array<int8_t>& array) {
	std::vector<uint8_t> bytes(static_cast<size_t>(array.length()));
	for (int32_t i = 0; i < array.length(); i++)
		bytes[static_cast<size_t>(i)] = static_cast<uint8_t>(array.get(i));
	return bytes;
}

/** Java: AbyssRankEnum.getId() - GRADE9_SOLDIER(1) ... SUPREME_COMMANDER(18), the ordinal + 1 (the enum companion of P5-01 does not exist) */
int32_t abyssRankId(utils::stats::AbyssRankEnum rank) {
	return static_cast<int32_t>(rank) + 1;
}

/** Java: AbyssRankEnum.getGpLossPerDay() - RankingConfig.TOP_RANKING_GP_LOSS.getOrDefault(this, 0) (the enum companion does not exist) */
int32_t gpLossPerDay(utils::stats::AbyssRankEnum rank) {
	auto gpLoss = configs::main::RankingConfig::TOP_RANKING_GP_LOSS.get();
	if (!gpLoss)
		return 0;
	// the config key is still the placeholder enum of configs/detail/ConfigEnums.h (same constants in ordinal order)
	auto it = gpLoss->find(static_cast<configs::detail::AbyssRankEnum>(static_cast<int32_t>(rank)));
	return it == gpLoss->end() ? 0 : it->second;
}

} // namespace

void PlayerEnterWorldService::enterWorld(AionConnection* client, int32_t objectId) {
	Ptr<Account> account = client->getAccount();
	Ptr<PlayerAccountData> playerAccData = account->getPlayerAccountData(objectId);
	if (!playerAccData) {
		log.warn("Player enterWorld fail: character obj ID {} was not found on account ID {}.", objectId, account->getId());
		client->sendPacket(SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::CONNECTION_ERROR));
		return;
	}

	Ptr<PlayerCommonData> pcd = playerAccData->getPlayerCommonData();
	if (!pcd) {
		log.warn("Player enterWorld fail: CommonData for character obj ID {} is null.", objectId);
		client->sendPacket(SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::CONNECTION_ERROR));
		return;
	}

	if (dao::PlayerDAO::isOnline(objectId)) { // char is still leaving the world and not saved yet (fast reentry from plastic surgery screen or packet hack)
		client->sendPacket(SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::REENTRY_TIME));
		return;
	}
	std::optional<int32_t> onlinePlayerId;
	for (Ptr<PlayerAccountData> p : account->getPlayerAccDataList()) {
		if (p->getPlayerCommonData()->isOnline()) {
			onlinePlayerId = p->getPlayerCommonData()->getPlayerObjId();
			break;
		}
	}
	if (onlinePlayerId) { // a char was online during acc login (double login or client crash), so reload pcd, appearance and acc warehouse
		if (dao::PlayerDAO::isOnline(*onlinePlayerId)) { // the found char is still leaving the world, so the acc wh might still be outdated
			client->sendPacket(SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::REENTRY_TIME));
			return;
		}
		std::unique_ptr<PlayerAccountData> reloaded = AccountService::loadPlayerAccountData(*account, *onlinePlayerId);
		playerAccData = Ptr<PlayerAccountData>(*reloaded);
		if (*onlinePlayerId == objectId)
			pcd = playerAccData->getPlayerCommonData(); // refresh lastOnline for reentry time validation
		account->addPlayerAccountData(std::move(reloaded));
		account->setAccountWarehouse(AccountService::loadAccountWarehouse(*account));
	}

	if (world::World::getInstance().isInWorld(objectId)) {
		log.warn("Player enterWorld fail: Duplicate character obj ID {} found in world.", objectId);
		client->sendPacket(SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::CONNECTION_ERROR));
		return;
	}

	// check if char is banned
	Ptr<model::account::CharacterBanInfo> cbi = playerAccData->getCharBanInfo();
	if (cbi) {
		if (cbi->getEnd() >= commons::utils::currentTimeMillis() / 1000) {
			client->sendPacket(SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::CONNECTION_ERROR));
			return;
		} else {
			dao::PlayerPunishmentsDAO::unpunishPlayer(objectId, PunishmentService_PunishmentType::CHARBAN);
		}
	}

	// passkey check
	if (configs::main::SecurityConfig::PASSKEY_ENABLE.load() && !account->getCharacterPasskey()->isPass()) {
		account->getCharacterPasskey()->setConnectType(model::account::CharacterPasskey::ConnectType::ENTER);
		account->getCharacterPasskey()->setObjectId(objectId);
		bool isExistPasskey = dao::PlayerPasskeyDAO::existCheckPlayerPasskey(account->getId());
		client->sendPacket(network::aion::serverpackets::SM_CHARACTER_SELECT(!isExistPasskey ? 0 : 1));
		return;
	}

	std::optional<commons::database::Timestamp> lastOnline = pcd->getLastOnline();
	if (!pcd->isInEditMode() && lastOnline &&
		commons::utils::currentTimeMillis() - lastOnline->time_since_epoch().count() < (configs::main::GSConfig::CHARACTER_REENTRY_TIME.load() * 1000)) {
		client->sendPacket(SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::REENTRY_TIME));
		return;
	}

	// C++ only (m5a-plan.md S-05): the account warehouse is shared by every character of the account, so its actor may be another character of
	// this account that is online (a second CM_ENTER_WORLD on a connection that already has an active player). The breakers null the actor for
	// the player they drop; put the online character back (see PlayerService::restoreAccountWarehouseActor).
	Ptr<Player> previousWarehouseActor = PlayerService::accountWarehouseActor(*account);
	Ref<Player> player = PlayerService::getPlayer(objectId, account);
	// C++ only (m5a-plan.md S-05, LogoutBreakers.h "Callers"): the account warehouse retains the Player since getPlayer. Unless it entered the world,
	// the Player is dropped here (duplicate enter, multi-client reject, the catch below), so the logout breakers cut its C++-only edges.
	bool enteredWorld = false;
	auto breakers = runtime::finally([&player, &enteredWorld, &account, previousWarehouseActor]() noexcept {
		if (enteredWorld)
			return;
		model::gameobjects::player::LogoutBreakers::run(*player);
		PlayerService::restoreAccountWarehouseActor(*account, previousWarehouseActor, *player);
	});
	if (!enteringWorld.contains(objectId) && enteringWorld.add(objectId)) {
		// Java finally: enteringWorld.remove(objectId)
		auto removeEntering = runtime::finally([objectId]() noexcept {
			try {
				enteringWorld.remove(objectId);
			} catch (...) {
				log.errorCurrentException("Could not remove " + std::to_string(objectId) + " from the entering players");
			}
		});
		try {
			if (player->isStaff() || MultiClientingService::tryEnterWorld(*player, client)) {
				enterWorld(client, *player);
				enteredWorld = true;
			} else {
				SM_ENTER_WORLD_CHECK::Msg msg = configs::main::SecurityConfig::MULTI_CLIENTING_RESTRICTION_MODE.load() ==
						configs::main::SecurityConfig::MultiClientingRestrictionMode::SAME_FACTION
					? SM_ENTER_WORLD_CHECK::Msg::BOTH_FACTIONS
					: SM_ENTER_WORLD_CHECK::Msg::CONNECTION_ERROR;
				client->sendPacket(SM_ENTER_WORLD_CHECK(msg));
			}
		} catch (...) {
			player->getController().delete_();
			pcd->setOnline(false);
			dao::PlayerDAO::onlinePlayer(*player, false);
			player->setClientConnection(nullptr);
			client->setActivePlayer(nullptr);
			client->sendPacket(SM_ENTER_WORLD_CHECK(SM_ENTER_WORLD_CHECK::Msg::CONNECTION_ERROR));
			log.errorCurrentException("Error during enter world of " + player->toString());
		}
	}
}

bool PlayerEnterWorldService::addEnteringWorldForTests(int32_t objectId) {
	return !enteringWorld.contains(objectId) && enteringWorld.add(objectId);
}

bool PlayerEnterWorldService::removeEnteringWorldForTests(int32_t objectId) {
	return enteringWorld.remove(objectId);
}

void PlayerEnterWorldService::enterWorld(AionConnection* client, Player& player) {
	using network::aion::serverpackets::SM_ABYSS_RANK;
	using network::aion::serverpackets::SM_AFTER_TIME_CHECK_4_7_5;
	using network::aion::serverpackets::SM_BLOCK_LIST;
	using network::aion::serverpackets::SM_CHANNEL_INFO;
	using network::aion::serverpackets::SM_EMOTION_LIST;
	using network::aion::serverpackets::SM_FRIEND_LIST;
	using network::aion::serverpackets::SM_GAME_TIME;
	using network::aion::serverpackets::SM_GM_BOOKMARK_ADD;
	using network::aion::serverpackets::SM_HOUSE_SCRIPTS;
	using network::aion::serverpackets::SM_INSTANCE_INFO;
	using network::aion::serverpackets::SM_ITEM_COOLDOWN;
	using network::aion::serverpackets::SM_LEGION_DOMINION_LOC_INFO;
	using network::aion::serverpackets::SM_MESSAGE;
	using network::aion::serverpackets::SM_MOTION;
	using network::aion::serverpackets::SM_PLAYER_SPAWN;
	using network::aion::serverpackets::SM_PRICES;
	using network::aion::serverpackets::SM_QUEST_LIST;
	using network::aion::serverpackets::SM_RECIPE_COOLDOWN;
	using network::aion::serverpackets::SM_RECIPE_LIST;
	using network::aion::serverpackets::SM_SKILL_COOLDOWN;
	using network::aion::serverpackets::SM_SKILL_LIST;
	using network::aion::serverpackets::SM_STATS_INFO;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	using network::aion::serverpackets::SM_TITLE_INFO;
	using network::aion::serverpackets::SM_UI_SETTINGS;
	using network::aion::serverpackets::SM_UNK_3_5_1;
	Ptr<Account> account = player.getAccount();
	Ptr<PlayerCommonData> pcd = player.getCommonData();

	client->resetPingFailCount();
	activatePassiveSkillEffects(player); // before setClientConnection to avoid packet spam
	player.setClientConnection(client->sharedFromThis());
	if (!client->setActivePlayer(player))
		throw runtime::IllegalStateException("Couldn't set active player");
	pcd->setOnline(true);
	player.getFriendList().setStatus(model::gameobjects::player::FriendList::Status::ONLINE, *pcd);
	dao::PlayerDAO::onlinePlayer(player, true);
	dao::PlayerDAO::storeLastOnlineTime(player.getObjectId(), commons::database::Timestamp(std::chrono::milliseconds(commons::utils::currentTimeMillis())));
	log.info("Player " + player.getName() + " (" + account->toString() + ") logged on");
	pcd->setInEditMode(false);

	world::World::getInstance().storeObject(player);

	// change player position if he isn't allowed to spawn in the current zone
	if (validateFortressZone(player)) // only check vortex zone if fortress check was ok (otherwise, the player is already set to bind point)
		validateVortexZone(player);

	// if player skipped some levels offline, learn missing skills and stuff
	player.getController().onLevelChange(dao::PlayerDAO::getOldCharacterLevel(player.getObjectId()), player.getLevel());

	// Energy of Repose must be calculated before sending SM_STATS_INFO
	if (pcd->getLastOnline()) {
		int64_t secondsOffline = (commons::utils::currentTimeMillis() - pcd->getLastOnline()->time_since_epoch().count()) / 1000;
		if (secondsOffline > 10 * 60) // 10 mins offline = 0 salvation points
			pcd->resetSalvationPoints();

		updateEnergyOfRepose(player, secondsOffline);

		if (secondsOffline > 5 * 60)
			pcd->setDp(0);
	}

	client->sendPacket(SM_HOUSE_SCRIPTS(0, model::house::PlayerScript::LUA_SANDBOX_FIX)); // client executes this immediately (scary, right?!)
	client->sendPacket(SM_UNK_3_5_1());
	StigmaService::onPlayerLogin(player);
	client->sendPacket(SM_ENTER_WORLD_CHECK());

	instance::InstanceService::onPlayerLogin(player);
	// Update player skills first!!!
	if (player.hasAccess(configs::administration::AdminConfig::GM_SKILLS.load()))
		utils::audit::GMService::getInstance().addGmSkills(player);
	abyss::AbyssSkillService::updateSkills(player);
	{
		std::vector<Ref<model::skill::PlayerSkillEntry>> skills;
		for (Ptr<model::skill::PlayerSkillEntry> skill : player.getSkillList()->getAllSkills())
			skills.emplace_back(skill);
		utils::collections::DynamicServerPacketBodySplitList<model::skill::PlayerSkillEntry> skillEntrySplitList(std::move(skills), false,
			SM_SKILL_LIST::STATIC_BODY_SIZE, network::aion::skillinfo::SkillEntryWriter::DYNAMIC_BODY_PART_SIZE_CALCULATOR);
		for (utils::collections::ListPart<model::skill::PlayerSkillEntry>& part : skillEntrySplitList)
			utils::PacketSendUtility::sendPacket(player, SM_SKILL_LIST(part.borrowed()));
	}
	if (Ptr<runtime::RcConcurrentHashMap<int32_t, int64_t>> skillCoolDowns = player.getSkillCoolDowns()) {
		std::unordered_map<int32_t, int64_t> cooldowns;
		for (const auto& entry : skillCoolDowns->snapshot())
			cooldowns.emplace(entry.key, entry.value);
		client->sendPacket(SM_SKILL_COOLDOWN(player, cooldowns, false));
	}

	if (!player.getItemCoolDowns().isEmpty()) {
		std::unordered_map<int32_t, Ptr<model::items::ItemCooldown>> itemCooldowns;
		for (const auto& entry : player.getItemCoolDowns().snapshot())
			itemCooldowns.emplace(entry.key, entry.value);
		client->sendPacket(SM_ITEM_COOLDOWN(itemCooldowns));
	}

	questEngine::QuestEngine::getInstance().sendCompletedQuests(player);
	client->sendPacket(SM_QUEST_LIST(player.getQuestStateList()->getUncompletedQuests()));
	client->sendPacket(SM_TITLE_INFO(pcd->getTitleId()));
	if (pcd->getBonusTitleId() != 0) {
		player.getTitleList().setBonusTitle(pcd->getBonusTitleId());
	}
	{
		std::vector<Ptr<model::gameobjects::player::motion::Motion>> motions;
		if (Ptr<runtime::RcLinkedHashMap<int32_t, Ref<model::gameobjects::player::motion::Motion>>> motionMap = player.getMotions().getMotions()) {
			for (const auto& entry : motionMap->snapshot())
				motions.emplace_back(entry.value);
		}
		client->sendPacket(SM_MOTION(motions));
	}
	client->sendPacket(SM_AFTER_TIME_CHECK_4_7_5()); // it is also sent after enter world check

	Ptr<runtime::Array<int8_t>> uiSettings = player.getPlayerSettings()->getUiSettings();
	Ptr<runtime::Array<int8_t>> shortcuts = player.getPlayerSettings()->getShortcuts();
	Ptr<runtime::Array<int8_t>> houseBuddies = player.getPlayerSettings()->getHouseBuddies();

	if (uiSettings)
		client->sendPacket(SM_UI_SETTINGS(bytesOf(*uiSettings), 0));

	if (shortcuts)
		client->sendPacket(SM_UI_SETTINGS(bytesOf(*shortcuts), 1));

	if (houseBuddies)
		client->sendPacket(SM_UI_SETTINGS(bytesOf(*houseBuddies), 2));

	sendItemInfos(client, player);

	client->sendPacket(SM_CHANNEL_INFO(player.getPosition()));

	KiskService::getInstance().onLogin(player);
	teleport::TeleportService::sendObeliskBindPoint(player);
	teleport::TeleportService::sendKiskBindPoint(player);

	panesterra::PanesterraService::getInstance().onEnterPanesterra(player);

	// ----------------------------- Retail sequence -----------------------------
	client->sendPacket(SM_PLAYER_SPAWN(player));
	// SM_WEATHER miss on login (but he 'live' in CM_LEVEL_READY.. need investigate)
	client->sendPacket(SM_GAME_TIME());
	if (player.isLegionMember())
		LegionService::getInstance().onLogin(player);
	sendWarehouseItemInfos(client, player);
	client->sendPacket(SM_TITLE_INFO(player));
	client->sendPacket(SM_EMOTION_LIST(static_cast<int8_t>(0), player.getEmotions()->getEmotions()));
	// SM_BD_UNK h 0
	SiegeService::getInstance().onPlayerLogin(player);
	client->sendPacket(SM_PRICES());
	if (!player.getCraftCooldowns()->isEmpty())
		client->sendPacket(SM_RECIPE_COOLDOWN(player, 1));
	teleport::BindPointTeleportService::onLogin(player);
	client->sendPacket(SM_FRIEND_LIST());
	client->sendPacket(SM_BLOCK_LIST());
	if (configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.load()) {
		AutoGroupService::getInstance().onPlayerLogin(player);
	}
	client->sendPacket(SM_INSTANCE_INFO(static_cast<int8_t>(2), player));
	client->sendPacket(SM_ABYSS_RANK(player));
	client->sendPacket(SM_STATS_INFO(player));
	// ----------------------------- Retail sequence -----------------------------

	if (player.hasAccess(configs::administration::AdminConfig::REVISION_INFO_ON_LOGIN.load())) {
		// Java: PacketSendUtility.sendMessage(player, VERSION_INFO, ChatType.WHITE) with VERSION_INFO = "Server " +
		// GameServer.versionInfo.getBuildInfo(GSConfig.TIME_ZONE_ID); GameServer.versionInfo belongs to P5-14 (GameServer.h, stage 2)
		AION_PARTIAL("VERSION_INFO for staff logins needs GameServer.versionInfo (m5a-plan.md O-03)");
	}

	if (std::shared_ptr<const std::vector<std::string>> membershipTypes = configs::main::MembershipConfig::MEMBERSHIP_TYPES.get();
		account->getMembership() > 0 && membershipTypes && account->getMembership() <= static_cast<int32_t>(membershipTypes->size())) {
		const std::string& accountType = (*membershipTypes)[static_cast<size_t>(account->getMembership() - 1)];
		client->sendPacket(SM_MESSAGE(0, "", "Your account is " + accountType, model::ChatType::GOLDEN_YELLOW)); // Java: null sender name
	}

	// Alliance Packet after SetBindPoint
	model::team::alliance::PlayerAllianceService::onPlayerLogin(player);

	PunishmentService::updatePrisonStatus(player);

	model::team::group::PlayerGroupService::onPlayerLogin(player);
	toypet::PetService::getInstance().onPlayerLogin(player);

	// ----------------------------- Retail sequence -----------------------------
	client->sendPacket(SM_LEGION_DOMINION_LOC_INFO());
	mail::MailService::onPlayerLogin(player);
	HousingBidService::getInstance().onPlayerLogin(player); // must ensure player mailbox is initialized first
	AtreianPassportService::getInstance().onLogin(player);
	sendMacroList(client, player);
	{
		std::unordered_set<int32_t> recipeIds;
		for (int32_t recipeId : player.getRecipeList()->getRecipeList().snapshot())
			recipeIds.insert(recipeId);
		client->sendPacket(SM_RECIPE_LIST(recipeIds));
	}
	BrokerService::getInstance().onPlayerLogin(player);
	HousingService::getInstance().onPlayerLogin(player); // must ensure player mailbox is initialized first
	// ----------------------------- Retail sequence -----------------------------
	if (configs::main::CustomConfig::ENABLE_SIMPLE_2NDCLASS.load())
		ClassChangeService::showClassChangeDialog(player);

	utils::audit::GMService::getInstance().onPlayerLogin(player);
	for (const Ref<dao::BookmarkDAO::Bookmark>& bookmark : dao::BookmarkDAO::loadBookmarks(player.getObjectId()))
		utils::PacketSendUtility::sendPacket(player, SM_GM_BOOKMARK_ADD(*bookmark));

	if (abyssRankId(player.getAbyssRank()->getRank()) >= abyssRankId(utils::stats::AbyssRankEnum::STAR1_OFFICER)) {
		client->sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_GLORY_POINT_LOSE_COMMON());
		client->sendPacket(SM_SYSTEM_MESSAGE::STR_MSG_GLORY_POINT_LOSE_PERSONAL(player.getName(), gpLossPerDay(player.getAbyssRank()->getRank())));
	}

	// Trigger restore services on login.
	player.getLifeStats()->updateCurrentStats();
	player.getObserveController()->notifyHPChangeObservers(player.getLifeStats()->getCurrentHp());

	if (configs::main::HTMLConfig::ENABLE_HTML_WELCOME.load()) {
		// Java passes the cached html, null if the file is missing
		std::optional<std::string> welcome = cache::HTMLCache::getInstance().getHTML("welcome.xhtml");
		HTMLService::showHTML(player, welcome ? *welcome : std::string());
	}

	reward::AdventService::getInstance().onLogin(player);

	player.getNpcFactions().sendDailyQuest();

	if (configs::main::HTMLConfig::ENABLE_GUIDES.load())
		HTMLService::onPlayerLogin(player);

	player.getEquipment().checkRankLimitItems(); // Remove items after offline changed rank

	std::vector<Ptr<model::Expirable>> expirables;
	for (size_t i = 0; i <= static_cast<size_t>(model::items::storage::StorageType::MAILBOX); i++) { // Java: StorageType.values()
		const auto st = static_cast<model::items::storage::StorageType>(i);
		if (st == model::items::storage::StorageType::LEGION_WAREHOUSE)
			continue;
		Ptr<model::items::storage::Storage> storage = player.getStorage(model::items::storage::getId(st));
		if (storage) {
			for (Ptr<model::gameobjects::Item> item : storage->getItems())
				expirables.emplace_back(*item);
		}
	}
	for (Ptr<model::gameobjects::Item> item : player.getEquipment().getEquippedItems())
		expirables.emplace_back(*item);
	if (Ptr<runtime::RcLinkedHashMap<int32_t, Ref<model::gameobjects::player::motion::Motion>>> motionMap = player.getMotions().getMotions()) {
		for (const auto& entry : motionMap->snapshot())
			expirables.emplace_back(*entry.value);
	}
	for (Ptr<model::gameobjects::player::emotion::Emotion> emotion : player.getEmotions()->getEmotions())
		expirables.emplace_back(*emotion);
	for (Ptr<model::gameobjects::player::title::Title> title : player.getTitleList().getTitles())
		expirables.emplace_back(*title);
	taskmanager::tasks::ExpireTimerTask::getInstance().registerExpirables(expirables, player);

	if (Ptr<model::house::House> activeHouse = player.getActiveHouse()) {
		for (Ptr<model::gameobjects::HouseObject> obj : activeHouse->getRegistry()->getObjects()) {
			if (obj->getPersistentState() != model::gameobjects::Persistable_PersistentState::DELETED)
				taskmanager::tasks::ExpireTimerTask::getInstance().registerExpirable(*obj, player);
		}
	}

	// scheduler periodic update
	const int32_t generalPeriod = configs::main::PeriodicSaveConfig::PLAYER_GENERAL.load() * 1000;
	player.getController().addTask(model::TaskId::PLAYER_UPDATE,
		utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(GeneralUpdateTask{{}, player.getObjectId()}, generalPeriod, generalPeriod));
	const int32_t itemsPeriod = configs::main::PeriodicSaveConfig::PLAYER_ITEMS.load() * 1000;
	player.getController().addTask(model::TaskId::INVENTORY_UPDATE,
		utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(ItemUpdateTask{{}, player.getObjectId()}, itemsPeriod, itemsPeriod));

	SurveyService::getInstance().showAvailable(player);
	event::EventService::getInstance().onPlayerLogin(player);

	if (configs::main::CraftConfig::DELETE_EXCESS_CRAFT_ENABLE.load())
		craft::RelinquishCraftStatus::removeExcessCraftStatus(player, false);

	// try to send bonus pack (if mailbox was full on lvlup)
	BonusPackService::getInstance().addPlayerCustomReward(player);
	FactionPackService::getInstance().addPlayerCustomReward(player);
	reward::VeteranRewardService::getInstance().tryReward(player);

	custom::pvpmap::PvpMapService::getInstance().onLogin(player);
}

void PlayerEnterWorldService::updateEnergyOfRepose(Player& player, int64_t secondsOffline) {
	player.getCommonData()->updateMaxRepose();
	if (player.getCommonData()->isReadyForReposeEnergy() && secondsOffline > 4 * 3600) { // more than 4 hours offline: start counting Repose Energy
																																														// addition
		double hours = static_cast<double>(secondsOffline) / 3600.0;
		// 48 hours offline = 100% Repose Energy (~1% each 30mins source: http://forums.na.aiononline.com/na/showthread.php?t=105940)
		int64_t addReposeEnergy = static_cast<int64_t>(std::floor((hours / 48) * static_cast<double>(player.getCommonData()->getMaxReposeEnergy()) + 0.5));
		// Additional Energy of Repose bonus if inside house
		Ptr<model::house::House> house = player.getActiveHouse();
		if (house) {
			const model::templates::housing::HouseAddress* hPos = house->getAddress();
			if (player.getWorldId() == hPos->getMapId() &&
				utils::PositionUtil::isInRange(player.getX(), player.getY(), player.getZ(), hPos->getX(), hPos->getY(), hPos->getZ(), 7))
				// Java: long *= float, i.e. (long) (addReposeEnergy * factor) in float arithmetic; apartment = 5% bonus, other houses 10%
				addReposeEnergy = static_cast<int64_t>(static_cast<float>(addReposeEnergy) *
					(house->getHouseType() == model::templates::housing::HouseType::STUDIO ? 1.05f : 1.10f));
		}
		player.getCommonData()->addReposeEnergy(addReposeEnergy);
	}
}

void PlayerEnterWorldService::activatePassiveSkillEffects(Player& player) {
	for (Ptr<model::skill::PlayerSkillEntry> skillEntry : player.getSkillList()->getAllSkills()) {
		const skillengine::model::SkillTemplate* skillTemplate = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillEntry->getSkillId());
		if (skillTemplate == nullptr) // Java: NullPointerException at skillTemplate.isPassive()
			throw runtime::NullPointerException("Cannot invoke \"SkillTemplate.isPassive()\" because \"skillTemplate\" is null");
		if (skillTemplate->isPassive())
			skillengine::SkillEngine::getInstance().applyEffectDirectly(skillTemplate, skillEntry->getSkillLevel(), player, player);
	}
}

bool PlayerEnterWorldService::validateFortressZone(Player& player) {
	Ptr<model::siege::FortressLocation> fortress = SiegeService::getInstance().findFortress(player.getWorldId(), player.getX(), player.getY(), player.getZ());
	if (fortress && fortress->isVulnerable() && fortress->isEnemy(player)) {
		int64_t lastOnlineMillis = !player.getCommonData()->getLastOnline() ? 0 : player.getCommonData()->getLastOnline()->time_since_epoch().count();
		// only relocate if the player logged out before siege start (online enemies automatically get teleported outside the fortress)
		if (lastOnlineMillis < SiegeService::getInstance().getSiege(*fortress)->getStartTime()) {
			Ptr<model::gameobjects::player::BindPointPosition> bind = player.getBindPoint();
			if (bind) {
				world::World::getInstance().setPosition(player, bind->getMapId(), bind->getX(), bind->getY(), bind->getZ(), bind->getHeading());
			} else {
				const dataholders::PlayerInitialData::LocationData& start = dataholders::DataManager::PLAYER_INITIAL_DATA->getSpawnLocation(player.getRace());
				world::World::getInstance().setPosition(player, start.getMapId(), start.getX(), start.getY(), start.getZ(), start.getHeading());
			}
			return false;
		}
	}
	return true;
}

void PlayerEnterWorldService::validateVortexZone(Player& player) {
	Ptr<model::vortex::VortexLocation> loc = VortexService::getInstance().getLocationByWorld(player.getWorldId());
	if (loc && player.getRace() == loc->getInvadersRace()) {
		if (loc->isInsideLocation(player) && loc->isActive() && loc->getVortexController()->getPassedPlayers().containsKey(player.getObjectId()))
			return;

		int32_t mapId = loc->getHomeWorldId();
		float x = loc->getHomePoint()->getX();
		float y = loc->getHomePoint()->getY();
		float z = loc->getHomePoint()->getZ();
		int8_t h = loc->getHomePoint()->getHeading();
		world::World::getInstance().setPosition(player, mapId, x, y, z, h);
	}
}

void PlayerEnterWorldService::sendItemInfos(AionConnection* client, Player& player) {
	using network::aion::serverpackets::SM_INVENTORY_INFO;
	player.setCubeLimit();
	player.setWarehouseLimit();
	// items
	model::items::storage::Storage& inventory = player.getInventory();
	std::vector<Ref<model::gameobjects::Item>> allItems;
	if (inventory.getKinah() == 0) {
		inventory.increaseKinah(0); // create an empty object with value 0
	}
	allItems.emplace_back(inventory.getKinahItem()); // always included even with 0 count, and first in the packet !
	for (Ptr<model::gameobjects::Item> item : player.getEquipment().getEquippedItems())
		allItems.emplace_back(item);
	for (Ptr<model::gameobjects::Item> item : inventory.getItems())
		allItems.emplace_back(item);

	utils::collections::FixedElementCountSplitList<model::gameobjects::Item> inventoryItemSplitList(std::move(allItems), true, 10);
	for (utils::collections::ListPart<model::gameobjects::Item>& part : inventoryItemSplitList)
		client->sendPacket(SM_INVENTORY_INFO(part.isFirst(), part.borrowed(), player));
	client->sendPacket(SM_INVENTORY_INFO(false, {}, player));
}

void PlayerEnterWorldService::sendWarehouseItemInfos(AionConnection* client, Player& player) {
	using network::aion::serverpackets::SM_WAREHOUSE_INFO;
	namespace storage = model::items::storage;
	WarehouseService::sendWarehouseInfo(player, true);
	// from 30 to 49, from 60 to 79
	for (int32_t i = storage::PET_BAG_MIN - 2; i <= storage::HOUSE_WH_MAX; i++) {
		if (i >= 50 && i < storage::HOUSE_WH_MIN)
			continue;
		Ptr<storage::Storage> storageOfType = player.getStorage(i);
		if (!storageOfType || storageOfType->getItemsWithKinah().size() == 0) {
			client->sendPacket(SM_WAREHOUSE_INFO({}, i, 0, true, player)); // Java: null items
			continue;
		}
		std::vector<Ref<model::gameobjects::Item>> items;
		for (Ptr<model::gameobjects::Item> item : storageOfType->getItemsWithKinah())
			items.emplace_back(item);
		utils::collections::FixedElementCountSplitList<model::gameobjects::Item> warehouseItemSplitList(std::move(items), true, 10);
		int32_t storageType = i;
		for (utils::collections::ListPart<model::gameobjects::Item>& part : warehouseItemSplitList)
			client->sendPacket(SM_WAREHOUSE_INFO(part.borrowed(), storageType, 0, part.isFirst(), player));
		client->sendPacket(SM_WAREHOUSE_INFO({}, storageType, 0, false, player));
		client->sendPacket(SM_WAREHOUSE_INFO({}, i, 0, false, player));
	}
}

void PlayerEnterWorldService::sendMacroList(AionConnection* client, Player& player) {
	using network::aion::serverpackets::SM_MACRO_LIST;
	using model::gameobjects::player::Macros;
	std::vector<Ref<Macros::Macro>> macros;
	for (Ptr<Macros::Macro> macro : player.getMacros()->getAll())
		macros.emplace_back(macro);
	utils::collections::DynamicServerPacketBodySplitList<Macros::Macro> macroSplitList(std::move(macros), true, SM_MACRO_LIST::STATIC_BODY_SIZE,
		SM_MACRO_LIST::DYNAMIC_BODY_PART_SIZE_CALCULATOR);
	for (utils::collections::ListPart<Macros::Macro>& part : macroSplitList)
		utils::PacketSendUtility::sendPacket(player, SM_MACRO_LIST(player.getObjectId(), part.borrowed(), part.isFirst()));
	static_cast<void>(client); // Java: the parameter is unused (PacketSendUtility sends to the player's connection)
}

void GeneralUpdateTask::operator()() const {
	Ptr<Player> player = world::World::getInstance().getPlayer(playerId);
	if (player) {
		try {
			dao::AbyssRankDAO::storeAbyssRank(*player);
			dao::PlayerSkillListDAO::storeSkills(*player);
			dao::PlayerQuestListDAO::store(*player);
			dao::PlayerDAO::storePlayer(*player);
			for (Ptr<model::house::House> house : *player->getHouses())
				house->save();
		} catch (const std::exception&) { // Java: catch (Exception ex)
			generalUpdateTaskLog.errorCurrentException("Exception during periodic saving of player " + player->getName());
		}
	}
}

void ItemUpdateTask::operator()() const {
	Ptr<Player> player = world::World::getInstance().getPlayer(playerId);
	if (player) {
		try {
			dao::InventoryDAO::store(*player);
			dao::ItemStoneListDAO::save(*player);
		} catch (const std::exception&) { // Java: catch (Exception ex)
			itemUpdateTaskLog.errorCurrentException("Exception during periodic saving of player items " + player->getName());
		}
	}
}

} // namespace aion::gameserver::services::player
