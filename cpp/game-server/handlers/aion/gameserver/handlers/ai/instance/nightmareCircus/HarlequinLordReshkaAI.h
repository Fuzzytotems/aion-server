#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <initializer_list>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::nightmareCircus {

/**
 * The AI of Harlequin Lord Reshka ("harlequinlordreshka"): his spawn brings twelve terror boxes, 36 s later twelve nightmare boxes, and he
 * opens the boxes after 30 s and 60 s; his death takes the boxes.
 * <p>
 * Java: data/handlers/ai/instance/nightmareCircus/HarlequinLordReshkaAI.java, @AIName("harlequinlordreshka") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class HarlequinLordReshkaAI : public AggressiveNpcAI {
private:
	const runtime::Ref<runtime::Array<runtime::FutureRef>> openBoxesTasks = runtime::Array<runtime::FutureRef>::make(2); // Java: = new Future<?>[2]
	runtime::Field<runtime::FutureRef> spawnBoxesTask{};

public:
	explicit HarlequinLordReshkaAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDied() override;

	void handleDespawned() override;

private:
	void cancelTasks();

	void spawnTerrorsBox();

	void spawnNightmaresBox();

	void openBoxes();

	void despawnNpcs(std::initializer_list<int32_t> npcIds);
};

} // namespace aion::gameserver::handlers::ai::instance::nightmareCircus
