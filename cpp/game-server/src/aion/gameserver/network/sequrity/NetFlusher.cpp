#include "aion/gameserver/network/sequrity/NetFlusher.h"

#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::network::sequrity {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.sequrity.NetFlusher");

void NetFlusher::add(runtime::PinnedCallback<void()> runnable, int64_t interval) {
	// Java: the anonymous TimerTask (NetFlusher$1) running the captured Runnable; the callback carries its own pin
	utils::ThreadPoolManager::getInstance().scheduleAtFixedRate(runtime::Pin(),
		[runnable = std::move(runnable)] {
			try {
				runnable();
			} catch (const std::exception&) {
				log.errorCurrentException("Exception in NetFlusher task"); // Java: e.printStackTrace()
			}
		},
		interval, interval);
}

} // namespace aion::gameserver::network::sequrity
