#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a conquest offering's shugo ("conquest_offering_buff_npc"): it greets, the first player who uses it gets one of four buffs and it
 * leaves; unused, it leaves after 65 s.
 * <p>
 * Java: data/handlers/ai/ConquestOfferingBuffNpcAI.java, @AIName("conquest_offering_buff_npc").
 */
class ConquestOfferingBuffNpcAI : public ActionItemNpcAI {
public:
	explicit ConquestOfferingBuffNpcAI(Npc& owner) : ActionItemNpcAI(owner) {}

	void handleSpawned() override;

	void handleDied() override;

protected:
	void handleUseItemFinish(Player& player) override;

	void handleDespawned() override;

private:
	void cancelTask();
	void sendWakeUpMsg();
	void sendTalkedMsg();

	/** Java: private AtomicBoolean used = new AtomicBoolean(false) */
	runtime::AtomicBoolean used{AION_LOCK_CLASS(ConquestOfferingBuffNpcAI::used), false};
	/** Java: private Future<?> despawnTask */
	runtime::Field<runtime::FutureRef> despawnTask{};
};

} // namespace aion::gameserver::handlers::ai
