#include "aion/loginserver/service/PlayerTransferService.h"

#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/GameServerTable.h"
#include "aion/loginserver/controller/AccountController.h"
#include "aion/loginserver/dao/AccountDAO.h"
#include "aion/loginserver/dao/PlayerTransferDAO.h"
#include "aion/loginserver/model/Account.h"
#include "aion/loginserver/network/gameserver/GsConnection.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_PTRANSFER_RESPONSE.h"

namespace aion::loginserver::service {

using namespace std::chrono_literals;
using network::gameserver::GsConnection;
using network::gameserver::serverpackets::SM_PTRANSFER_RESPONSE;
using ptransfer::PlayerTransferRequest;
using ptransfer::PlayerTransferResultStatus;
using ptransfer::PlayerTransferStatus;
using ptransfer::PlayerTransferTask;

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.service.PlayerTransferService"));
	return *logger;
}

/** @return the connection of the game server, nullptr if the server does not exist or is not connected */
std::shared_ptr<GsConnection> connectionOf(const std::shared_ptr<GameServerInfo>& server) {
	return server ? server->getConnection() : nullptr;
}

[[noreturn]] void throwUnknownTask(int32_t taskId) {
	throw commons::utils::IllegalStateException("Unknown player transfer task #" + std::to_string(taskId));
}

} // namespace

PlayerTransferService& PlayerTransferService::getInstance() {
	static auto* instance = new PlayerTransferService(10s, 7min); // leaked: its thread is stopped by shutdown(), not by static destruction
	return *instance;
}

PlayerTransferService::PlayerTransferService(std::chrono::milliseconds initialDelay, std::chrono::milliseconds period) {
	scheduledExecutorService.scheduleAtFixedRate([this] { verifyNewTasks(); }, initialDelay, period);
	log().info("PlayerTransferService will be initialized in " + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(initialDelay).count()) +
		" sec.");
}

void PlayerTransferService::verifyNewTasks() {
	std::lock_guard lock(mutex);
	std::vector<PlayerTransferTask> tasksNew = dao::PlayerTransferDAO::getNew<PlayerTransferTask>();
	log().info("PlayerTransfer perform task init. " + std::to_string(tasks.size()) + " new tasks.");
	for (PlayerTransferTask& task : tasksNew) {
		std::shared_ptr<GameServerInfo> server = GameServerTable::getGameServerInfo(task.sourceServerId);
		std::shared_ptr<GsConnection> serverConnection = connectionOf(server);
		if (!serverConnection) {
			log().error("cannot perform transfer task #" + std::to_string(task.id) + " while source server is down #" + std::to_string(task.sourceServerId));
			continue;
		}

		std::shared_ptr<GameServerInfo> targetServer = GameServerTable::getGameServerInfo(task.targetServerId);
		if (!connectionOf(targetServer)) {
			log().error("cannot perform transfer task #" + std::to_string(task.id) + " while target server is down #" + std::to_string(task.targetServerId));
			continue;
		}

		if (server->isAccountOnGameServer(task.sourceAccountId)) {
			log().error("cannot perform transfer task #" + std::to_string(task.id) + " while source account is online " + std::to_string(task.sourceAccountId));
			continue;
		}

		if (targetServer->isAccountOnGameServer(task.targetAccountId)) {
			log().error("cannot perform transfer task #" + std::to_string(task.id) + " while target account is online " + std::to_string(task.targetAccountId));
			continue;
		}

		task.status = PlayerTransferTask::STATUS_ACTIVE;
		tasks.insert_or_assign(task.id, task);
		dao::PlayerTransferDAO::update(task);
		serverConnection->sendPacket(std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::PERFORM_ACTION, task));
		log().info("performing player transfer #" + std::to_string(task.id));
	}
}

void PlayerTransferService::shutdown() {
	scheduledExecutorService.shutdown();
	if (!scheduledExecutorService.isTerminated()) {
		log().info("Waiting for PlayerTransferService to finish...");
		scheduledExecutorService.awaitTermination(5s);
	}
}

void PlayerTransferService::requestTransfer(int32_t taskId, std::string name, std::vector<uint8_t> db) {
	std::lock_guard lock(mutex);
	auto taskIt = tasks.find(taskId);
	if (taskIt == tasks.end())
		throwUnknownTask(taskId);
	const PlayerTransferTask& task = taskIt->second;
	std::shared_ptr<GsConnection> targetConnection = connectionOf(GameServerTable::getGameServerInfo(task.targetServerId));
	if (!targetConnection) {
		log().error("Player transfer requests offline server! #" + std::to_string(task.targetServerId));
		return;
	}

	std::shared_ptr<GameServerInfo> server = GameServerTable::getGameServerInfo(task.sourceServerId);
	std::shared_ptr<GsConnection> serverConnection = connectionOf(server);
	if (!serverConnection) {
		log().error("Player transfer requests offline server! #" + std::to_string(task.sourceServerId));
		return;
	}

	if (GameServerTable::getGameServerInfo(task.targetServerId)->isAccountOnGameServer(task.targetAccountId)) {
		log().error("Player transfer cant be performed while target account is online at server #" + std::to_string(task.targetServerId) + ". " +
			std::to_string(task.targetAccountId));
		serverConnection->sendPacket(std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::ERROR_, taskId,
			"transfer cant be performed while target account is online at server"));
		return;
	}

	if (transfers.contains(taskId)) {
		log().error("Player transfer cant be performed while it is already active #" + std::to_string(task.targetServerId) + ". " +
			std::to_string(task.targetAccountId));
		serverConnection->sendPacket(
			std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::ERROR_, taskId, "transfer cant be performed while it is already active"));
		return;
	}

	std::shared_ptr<model::Account> account = controller::AccountController::loadAccount(task.targetAccountId);
	std::shared_ptr<model::Account> saccount = controller::AccountController::loadAccount(task.sourceAccountId);

	auto request = std::make_shared<PlayerTransferRequest>(PlayerTransferStatus::STEP1);
	request->serverId = task.sourceServerId;
	request->targetServerId = task.targetServerId;
	request->targetAccountId = task.targetAccountId;
	request->db = std::move(db);
	request->name = std::move(name);
	request->targetAccount = account;
	request->account = account;
	request->saccount = saccount;
	request->taskId = taskId;

	transfers.insert_or_assign(taskId, request);

	if (!account || !saccount)
		throw commons::utils::IllegalStateException("Player transfer #" + std::to_string(taskId) + " references a missing account");
	account->setActivated(0);
	saccount->setActivated(0);
	dao::AccountDAO::updateAccount(*account);
	dao::AccountDAO::updateAccount(*saccount);

	targetConnection->sendPacket(std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::SEND_INFO, std::shared_ptr<const PlayerTransferRequest>(request)));
	log().info("player transfer account " + std::to_string(task.targetServerId) + " became active.");
}

void PlayerTransferService::onTaskStop(int32_t taskId, std::string_view reason) {
	std::lock_guard lock(mutex);
	auto taskIt = tasks.find(taskId);
	if (taskIt == tasks.end())
		throwUnknownTask(taskId);
	PlayerTransferTask task = std::move(taskIt->second);
	tasks.erase(taskIt);
	task.status = PlayerTransferTask::STATUS_ERROR;
	task.comment = std::string(reason);
	dao::PlayerTransferDAO::update(task);
}

void PlayerTransferService::onError(int32_t taskId, std::string_view reason) {
	std::lock_guard lock(mutex);
	std::shared_ptr<PlayerTransferRequest> request;
	if (auto it = transfers.find(taskId); it != transfers.end()) {
		request = std::move(it->second);
		transfers.erase(it);
	}
	auto taskIt = tasks.find(taskId);
	if (taskIt == tasks.end())
		throwUnknownTask(taskId);
	PlayerTransferTask task = std::move(taskIt->second);
	tasks.erase(taskIt);
	task.status = PlayerTransferTask::STATUS_ERROR;
	task.comment = std::string(reason);
	dao::PlayerTransferDAO::update(task);
	if (!request)
		throwUnknownTask(taskId);
	std::shared_ptr<GsConnection> targetConnection = connectionOf(GameServerTable::getGameServerInfo(request->targetServerId));
	if (!targetConnection) {
		log().error("Player transfer requests offline server! #" + std::to_string(request->targetServerId));
		return;
	}

	request->account->setActivated(1);
	request->saccount->setActivated(1);
	dao::AccountDAO::updateAccount(*request->account);
	dao::AccountDAO::updateAccount(*request->saccount);

	targetConnection->sendPacket(std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::ERROR_, taskId, std::string(reason)));
}

void PlayerTransferService::onOk(int32_t taskId) {
	std::lock_guard lock(mutex);
	std::shared_ptr<PlayerTransferRequest> request;
	if (auto it = transfers.find(taskId); it != transfers.end()) {
		request = std::move(it->second);
		transfers.erase(it);
	}
	auto taskIt = tasks.find(taskId);
	if (taskIt == tasks.end())
		throwUnknownTask(taskId);
	PlayerTransferTask task = std::move(taskIt->second);
	tasks.erase(taskIt);
	task.status = PlayerTransferTask::STATUS_DONE;
	task.comment = "task done";
	dao::PlayerTransferDAO::update(task);
	if (!request)
		throwUnknownTask(taskId);
	std::shared_ptr<GsConnection> sourceConnection = connectionOf(GameServerTable::getGameServerInfo(request->serverId));
	if (!sourceConnection) {
		log().error("Player transfer requests offline server! #" + std::to_string(request->serverId));
		return;
	}
	request->account->setActivated(1);
	request->saccount->setActivated(1);
	dao::AccountDAO::updateAccount(*request->account);
	dao::AccountDAO::updateAccount(*request->saccount);
	log().info("transfer #" + std::to_string(taskId) + " went onOK!");
	sourceConnection->sendPacket(std::make_shared<SM_PTRANSFER_RESPONSE>(PlayerTransferResultStatus::OK, std::shared_ptr<const PlayerTransferRequest>(request)));
}

} // namespace aion::loginserver::service
