#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tallocsHollow {

/**
 * The AI of Celestius in Taloc's Hollow ("celestius"): the first hit by a player or summon starts the helper call - every 25 s a cast and
 * three helpers walking their routes; back home, despawned or dead she stops it and takes the helpers.
 * <p>
 * Java: data/handlers/ai/instance/tallocsHollow/CelestiusAI.java, @AIName("celestius") (the marker is in the .cpp).
 *
 * @author xTz
 */
class CelestiusAI : public AggressiveNpcAI {
private:
	static constexpr int32_t SUMMONS_ID = 281514;
	runtime::AtomicBoolean isSpawnTaskStarted{AION_LOCK_CLASS(CelestiusAI::isSpawnTaskStarted)}; // Java: = new AtomicBoolean()
	runtime::Field<runtime::FutureRef> helpersTask{};

public:
	explicit CelestiusAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void startHelpersCall();

	void startWalker(Npc& npc, std::string_view walkId);

	void cleanUp();

	void cancelTask();

	void deleteSummons();

protected:
	void handleBackHome() override;

	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::tallocsHollow
