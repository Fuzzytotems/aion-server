#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ChestAI.h"

#include "aion/commons/logging/Logger.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::events {

/**
 * The AI of the hidden birthday cakes ("hidden_cake"): a rider dismounts to open one; each use is counted (logged once an hour) and may
 * (25 %) bring a jester for two minutes.
 * <p>
 * Java: data/handlers/ai/events/HiddenBirthdayCakeAI.java, @AIName("hidden_cake") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class HiddenBirthdayCakeAI : public ChestAI {
private:
	/** Java: private final static Logger log = LoggerFactory.getLogger("EVENT_LOG") */
	static const commons::logging::Logger log;
	/** Java: private final static AtomicInteger collectedCakes = new AtomicInteger() */
	static runtime::AtomicInteger& collectedCakes();
	static constexpr int32_t JEST_SPAWN_CHANCE = 25;
	static inline const runtime::Ref<runtime::Array<int32_t>> JEST_SPAWN_IDS = runtime::Array<int32_t>::of({210341, 214732, 210595});
	/** Java: private static volatile long lastLogTime = System.currentTimeMillis() */
	static runtime::AtomicLong& lastLogTime();
	/** Java: private static volatile int lastCakeCount */
	static runtime::AtomicInteger& lastCakeCount();

public:
	explicit HiddenBirthdayCakeAI(Npc& owner) : ChestAI(owner) {}

private:
	void logCollectedCakes(int32_t cakes);

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::events
