#include "aion/gameserver/services/player/PlayerLeaveWorldService.h"

#include <algorithm>
#include <chrono>
#include <memory>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PetController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/dao/ItemCooldownsDAO.h"
#include "aion/gameserver/dao/PlayerCooldownsDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dao/PlayerEffectsDAO.h"
#include "aion/gameserver/dao/PlayerLifeStatsDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/chatserver/ChatServer.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/services/DuelService.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/services/KiskService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/services/RepurchaseService.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/services/findgroup/FindGroupService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/player/MultiClientService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"
#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/GMService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::player {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.player.PlayerLeaveWorldService");

using model::gameobjects::player::Player;
using runtime::Ptr;

runtime::ConcurrentHashMap<runtime::Ref<Player>, runtime::Ref<PlayerLeaveWorldService::LeaveWorldTask>> PlayerLeaveWorldService::leaveWorldTaskByPlayer{
	AION_LOCK_CLASS(PlayerLeaveWorldService::leaveWorldTaskByPlayer#stripe)};

void PlayerLeaveWorldService::registerLeaveWorld(Player& player) {
	int64_t lastActionTimeMillis = std::max(player.getMoveController()->getLastMoveUpdate(), player.getController().getLastCombatTime());
	int64_t millisSinceLastPlayerAction = commons::utils::currentTimeMillis() - lastActionTimeMillis;
	int64_t waitTimeMillis = std::chrono::milliseconds(std::chrono::seconds(10)).count();
	int64_t delayMillis = std::max<int64_t>(0, waitTimeMillis - millisSinceLastPlayerAction);
	leaveWorldTaskByPlayer.computeIfAbsent(runtime::Ref<Player>(player), [&player] { return LeaveWorldTask::create(player); })->scheduleOrRun(delayMillis);
}

bool PlayerLeaveWorldService::isLeavingWorld(int32_t playerObjectId) {
	for (const runtime::Ref<Player>& p : leaveWorldTaskByPlayer.keySet())
		if (p->getObjectId() == playerObjectId)
			return true;
	return false;
}

bool PlayerLeaveWorldService::isLeavingWorld(Player& player) {
	return leaveWorldTaskByPlayer.containsKey(runtime::Ref<Player>(player));
}

void PlayerLeaveWorldService::processPendingLeaveWorldTasks() {
	for (const runtime::Ref<LeaveWorldTask>& task : leaveWorldTaskByPlayer.values())
		task->run();
}

void PlayerLeaveWorldService::leaveWorld(Player& player) {
	// C++ only (runtime-architecture.md §5.3 RR-5): the logout breakers run on every exit, also when a call below throws
	auto breakers = runtime::finally([&player]() noexcept { model::gameobjects::player::LogoutBreakers::run(player); });
	std::shared_ptr<network::aion::AionConnection> con = player.getClientConnection();
	player.setClientConnection(nullptr); // this sets the player semi-offline, PacketSendUtility will not send packets anymore

	Ptr<world::WorldPosition> pos = player.getPosition();
	if (!pos || !pos->getMapRegion()) { // ensure safe logout
		log.warn("{} had invalid position: {} so he was reset to bind point", player.toString(), pos ? pos->toString() : std::string("null"));
		Ptr<model::gameobjects::player::BindPointPosition> bp = player.getBindPoint();
		runtime::Ref<world::WorldPosition> newPosition;
		if (bp)
			newPosition = world::World::getInstance().createPosition(bp->getMapId(), bp->getX(), bp->getY(), bp->getZ(), bp->getHeading(), 1);
		else {
			const dataholders::PlayerInitialData::LocationData& ld = dataholders::DataManager::PLAYER_INITIAL_DATA->getSpawnLocation(player.getRace());
			newPosition = world::World::getInstance().createPosition(ld.getMapId(), ld.getX(), ld.getY(), ld.getZ(), ld.getHeading(), 1);
		}
		player.setPosition(newPosition);
	}

	findgroup::FindGroupService::getInstance().onLogout(player);
	RecallService::getInstance().cancel(player, RecallService::CancelReason::CANCELLED);
	player.getResponseRequester().denyAll();
	player.getFriendList().setStatus(model::gameobjects::player::FriendList::Status::OFFLINE, *player.getCommonData());
	BrokerService::getInstance().removePlayerCache(player);
	ExchangeService::getInstance().cancelExchange(player);
	RepurchaseService::getInstance().removeRepurchaseItems(player);
	if (configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.load())
		AutoGroupService::getInstance().onLogout(player);
	conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().onLeaveMap(player);
	MultiClientService::onLeaveWorld(player);
	instance::InstanceService::onLogout(player);
	utils::audit::GMService::getInstance().onPlayerLogout(player);
	KiskService::getInstance().onLogout(player);

	if (player.isDead()) {
		if (player.isInInstance() || player.getWorldId() == 400030000)
			PlayerReviveService::instanceRevive(player);
		else
			PlayerReviveService::bindRevive(player);
	} else if (DuelService::getInstance().isDueling(player)) {
		DuelService::getInstance().loseDuel(player);
	}
	player.getEffectController()->removeNonStorableEffectsForLogout();
	dao::PlayerEffectsDAO::storePlayerEffects(player);
	dao::ItemCooldownsDAO::storeItemCooldowns(player);
	dao::PlayerLifeStatsDAO::updatePlayerLifeStat(player);

	model::team::group::PlayerGroupService::onPlayerLogout(player);
	model::team::alliance::PlayerAllianceService::onPlayerLogout(player);
	// fix legion warehouse exploits
	LegionService::getInstance().LegionWhUpdate(player);
	player.getEffectController()->removeAllEffects(true);
	player.getLifeStats()->cancelAllTasks();

	Ptr<model::gameobjects::Summon> summon = player.getSummon();
	if (summon)
		summons::SummonsService::release(*summon, model::summons::UnsummonType::LOGOUT); // puts the summoning skill on cooldown, so store cooldowns afterwards
	dao::PlayerCooldownsDAO::storePlayerCooldowns(player);
	if (player.getPet())
		player.getPet()->getController().delete_();
	if (player.getPostman())
		player.getPostman()->getController().delete_();

	taskmanager::tasks::ExpireTimerTask::getInstance().unregisterExpirables(player);
	if (player.getInteractionTask())
		player.getInteractionTask()->abort();

	runtime::Ref<questEngine::model::QuestEnv> questEnv = questEngine::model::QuestEnv::create(nullptr, player, 0);
	questEngine::QuestEngine::getInstance().onLogOut(*questEnv);
	const commons::database::Timestamp lastOnline{std::chrono::milliseconds(commons::utils::currentTimeMillis())};
	player.getController().delete_();
	player.getCommonData()->setOnline(false);
	player.getCommonData()->setLastOnline(lastOnline);
	if (player.isLegionMember()) // must be called after setOnline and setLastOnline
		LegionService::getInstance().onLogout(player);
	player.getCommonData()->setX(player.getX());
	player.getCommonData()->setY(player.getY());
	player.getCommonData()->setZ(player.getZ());
	player.getCommonData()->setHeading(player.getHeading());

	network::chatserver::ChatServer::getInstance().sendPlayerLogout(player);

	PlayerService::storePlayer(player);

	player.getInventory().setOwner(nullptr);
	player.getWarehouse().setOwner(nullptr);
	player.getAccount()->getAccountWarehouse().setOwner(nullptr);

	dao::PlayerDAO::storeOldCharacterLevel(player.getObjectId(), player.getLevel());
	dao::PlayerDAO::storeLastOnlineTime(player.getObjectId(), lastOnline);
	dao::PlayerDAO::onlinePlayer(player, false);

	if (!con) // Java: NullPointerException
		throw runtime::NullPointerException("Cannot invoke \"AionConnection.setActivePlayer(Player)\" because \"con\" is null");
	con->setActivePlayer(nullptr);
}

PlayerLeaveWorldService::LeaveWorldTask::LeaveWorldTask(Player& playerValue) : player(playerValue) {
}

runtime::Ref<PlayerLeaveWorldService::LeaveWorldTask> PlayerLeaveWorldService::LeaveWorldTask::create(Player& player) {
	return runtime::makeRef<LeaveWorldTask>(player);
}

// lint: L7 Java's synchronized scheduleOrRun (fieldmap.py counts it under the name "schedule")
void PlayerLeaveWorldService::LeaveWorldTask::scheduleOrRun(int64_t delayMillis) {
	SYNCHRONIZED(*this) {
		if (finished.get() || future.get()) // lint: L12 Field<FutureRef>::get reads the field, it does not wait for the task
			return;
		if (delayMillis <= 0)
			run();
		else
			future.set(utils::ThreadPoolManager::getInstance().schedule(this, &LeaveWorldTask::run, delayMillis));
	}
}

void PlayerLeaveWorldService::LeaveWorldTask::run() {
	SYNCHRONIZED(*this) {
		if (finished.get())
			return;
		finished.set(true);
		// Java: finally { leaveWorldTaskByPlayer.remove(player, this); }
		auto removeTask = runtime::finally([this]() noexcept {
			try {
				leaveWorldTaskByPlayer.remove(player, runtime::Ref<LeaveWorldTask>(*this));
			} catch (...) {
				log.errorCurrentException("Could not unregister the leave world task of " + player->toString());
			}
		});
		try {
			if (player->isOnline())
				leaveWorld(*player);
			if (runtime::FutureRef pending = future.get()) // lint: L12 Field<FutureRef>::get reads the field, it does not wait for the task
				pending->cancel(false);
		} catch (...) {
			log.errorCurrentException("Error while processing leave world task for " + player->toString());
		}
	}
}

PlayerLeaveWorldService::LeaveWorldTask::~LeaveWorldTask() = default;

} // namespace aion::gameserver::services::player
