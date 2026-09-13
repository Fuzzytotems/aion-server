#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>

#include "aion/loginserver/utils/ScheduledExecutor.h"

namespace aion::loginserver::network::gameserver {
class GsConnection;
}

namespace aion::loginserver {

/**
 * Pings an authenticated game server every PERIOD (5 seconds) and closes the connection if more than three pings in a row were not answered
 * (CM_GS_PONG resets the counter): pings are sent at 5, 10 and 15 seconds, the connection is closed at 20 seconds.
 * <p>
 * The task is a member of its GsConnection. The scheduled job references the connection weakly, so it does not keep it alive; stop() (called in
 * GsConnection::onDisconnect) cancels it. Thread safe.
 * <p>
 * Java: com.aionemu.loginserver.PingPongTask
 *
 * @author KID, Neon
 */
class PingPongTask {
public:
	/** C++ addition: the ping interval (Java: fixed 5 seconds). Only tests change it, before game servers connect. */
	static inline std::atomic<std::chrono::milliseconds> PERIOD = std::chrono::milliseconds(5000);

	explicit PingPongTask(network::gameserver::GsConnection& connection) noexcept : connection(connection) {}

	PingPongTask(const PingPongTask&) = delete;
	PingPongTask& operator=(const PingPongTask&) = delete;

	/** Sends SM_PING, or closes the connection if the game server did not answer the last three pings. */
	void run();

	void onReceivePong() noexcept;

	/**
	 * Schedules run() at a fixed rate of PERIOD, starting after PERIOD.
	 * @throws commons::utils::UnsupportedOperationException "PingPongTask was already started"
	 */
	void start(utils::ScheduledExecutor& scheduledExecutorService);

	void stop();

private:
	network::gameserver::GsConnection& connection;
	std::atomic<int32_t> unrespondedPingCount = 0;
	std::mutex mutex;
	std::shared_ptr<utils::ScheduledExecutor::ScheduledFuture> task; // guarded by mutex
};

} // namespace aion::loginserver
