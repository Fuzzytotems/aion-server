#include "aion/loginserver/PingPongTask.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/loginserver/GameServerInfo.h"
#include "aion/loginserver/network/gameserver/GsConnection.h"
#include "aion/loginserver/network/gameserver/serverpackets/SM_PING.h"

namespace aion::loginserver {

using network::gameserver::GsConnection;

void PingPongTask::run() {
	if (unrespondedPingCount.fetch_add(1) <= 2) {
		connection.sendPacket(std::make_shared<network::gameserver::serverpackets::SM_PING>());
	} else {
		stop();
		std::shared_ptr<GameServerInfo> gsi = connection.getGameServerInfo();
		commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.PingPongTask")
			.warn("Gameserver #" + (gsi ? std::to_string(gsi->getId()) : std::string("null")) + " connection died, closing it.");
		connection.close();
	}
}

void PingPongTask::onReceivePong() noexcept {
	unrespondedPingCount.store(0);
}

void PingPongTask::start(utils::ScheduledExecutor& scheduledExecutorService) {
	std::lock_guard lock(mutex);
	if (task)
		throw commons::utils::UnsupportedOperationException("PingPongTask was already started");
	std::weak_ptr<GsConnection> weakConnection = connection.sharedFromThis();
	std::chrono::milliseconds period = PERIOD.load();
	task = scheduledExecutorService.scheduleAtFixedRate(
		std::function<void(utils::ScheduledExecutor::ScheduledFuture&)>([weakConnection](utils::ScheduledExecutor::ScheduledFuture& future) {
			std::shared_ptr<GsConnection> con = weakConnection.lock();
			if (con && !con->isClosed())
				con->getPingPongTask().run();
			else
				future.cancel(); // stop() may have been called before start() (disconnect during the authentication)
		}),
		period, period);
}

void PingPongTask::stop() {
	std::lock_guard lock(mutex);
	if (task)
		task->cancel();
}

} // namespace aion::loginserver
