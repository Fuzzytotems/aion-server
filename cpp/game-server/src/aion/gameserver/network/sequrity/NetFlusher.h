#pragma once

#include <cstdint>

#include "aion/gameserver/network/sequrity/fwd.h"
#include "aion/gameserver/runtime/sched/PinnedCallback.h"

namespace aion::gameserver::network::sequrity {

/**
 * C++: the periodic tasks run on the ThreadPoolManager's scheduled pool instead of Java's own daemon Timer thread (game code creates no threads,
 * conventions-game-server.md); an exception of one run is logged and the task keeps running, like Java's catch of RuntimeException.
 *
 * @author NB4L1
 */
class NetFlusher final {
public:
	NetFlusher() = delete;

	static void add(runtime::PinnedCallback<void()> runnable, int64_t interval);
};

} // namespace aion::gameserver::network::sequrity
