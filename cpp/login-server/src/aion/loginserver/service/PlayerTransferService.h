#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/loginserver/service/ptransfer/PlayerTransferRequest.h"
#include "aion/loginserver/service/ptransfer/PlayerTransferTask.h"
#include "aion/loginserver/utils/ScheduledExecutor.h"

namespace aion::loginserver::service {

/**
 * Performs the player transfers stored in the player_transfers table: new tasks are checked 10 seconds after startup and then every 7 minutes, and
 * sent to the source game server, which answers via CM_PTRANSFER_CONTROL.
 * <p>
 * <b>Threads.</b> The maps of active tasks and transfers are used by the scheduler thread and game server packet threads (Java: unsynchronized
 * HashMaps). Every public method holds the service's mutex for its whole duration, so each check-then-act sequence is atomic. While holding it, the
 * service calls the DAOs, AccountController::loadAccount and sendPacket; it is never taken by code that holds a connection guard.
 * <p>
 * Java: com.aionemu.loginserver.service.PlayerTransferService
 *
 * @author KID
 */
class PlayerTransferService {
public:
	/** Java: getInstance() - the service is created on first use (LoginServer startup) and logs "PlayerTransferService will be initialized in 10 sec." */
	static PlayerTransferService& getInstance();

	/**
	 * C++ addition: creates a service with its own scheduler (getInstance() uses 10 seconds and 7 minutes). Tests create their own instances.
	 */
	PlayerTransferService(std::chrono::milliseconds initialDelay, std::chrono::milliseconds period);

	PlayerTransferService(const PlayerTransferService&) = delete;
	PlayerTransferService& operator=(const PlayerTransferService&) = delete;

	/** Stops the scheduler, waiting up to 5 seconds for a running check ("Waiting for PlayerTransferService to finish..."). */
	void shutdown();

	/** Starts the new tasks of the database whose game servers are online and whose accounts are offline (Java: protected). */
	void verifyNewTasks();

	/**
	 * sended from source server to login with character information
	 * @throws commons::utils::IllegalStateException for an unknown task id or account (Java: NullPointerException)
	 */
	void requestTransfer(int32_t taskId, std::string name, std::vector<uint8_t> db);

	/**
	 * When source server refuse to do transfer with reason
	 * @throws commons::utils::IllegalStateException for an unknown task id (Java: NullPointerException)
	 */
	void onTaskStop(int32_t taskId, std::string_view reason);

	/**
	 * response from target server after cloning character
	 * @throws commons::utils::IllegalStateException for an unknown task id (Java: NullPointerException)
	 */
	void onError(int32_t taskId, std::string_view reason);

	/**
	 * response from target server after cloning character
	 * @throws commons::utils::IllegalStateException for an unknown task id (Java: NullPointerException)
	 */
	void onOk(int32_t taskId);

private:
	std::recursive_mutex mutex;
	std::unordered_map<int32_t, std::shared_ptr<ptransfer::PlayerTransferRequest>> transfers;
	std::unordered_map<int32_t, ptransfer::PlayerTransferTask> tasks;
	utils::ScheduledExecutor scheduledExecutorService{"PlayerTransferService"};
};

} // namespace aion::loginserver::service
