#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Brigade General Tahabata in Tiamat Stronghold ("brigadegeneraltahabata"): his first attack closes door 610 behind the players. His
 * phases: 96 % a lava eruption (20756, a 283051, 10 s later the floors 283116/283117) and the Piercing Strike every 20 s (from 15 s: he jumps to
 * a random hated player within 40 m and casts 20754, then 20755 3 s later if it hit), 75 % 20761 and the fire storm every 20 s (from 10 s: 20758
 * and one to four 283045 10 m around him while none stands), 60/40/25 % 20761, 55 % a lava eruption (283118/283119), 20 % both tasks stop and
 * a last eruption (283120/283121), 10 % 20942, 7 % 20883 and a 283102. Back home, dead or despawned the tasks stop; back home or dead the door
 * opens again and the floors go.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/BrigadeGeneralTahabataAI.java, @AIName("brigadegeneraltahabata") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Luzien
 */
class BrigadeGeneralTahabataAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	// Java: = new HpPhases(96, 75, 60, 55, 40, 25, 20, 10, 7)
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(96, {75, 60, 55, 40, 25, 20, 10, 7});
	runtime::AtomicBoolean isHome{AION_LOCK_CLASS(BrigadeGeneralTahabataAI::isHome), true}; // Java: = new AtomicBoolean(true)
	runtime::Field<runtime::FutureRef> piercingStrikeTask{};
	runtime::Field<runtime::FutureRef> fireStormTask{};

public:
	explicit BrigadeGeneralTahabataAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

private:
	void startPiercingStrikeTask();

	void startFireStormTask();

	void startFireStormEvent();

	void startPiercingStrikeEvent();

	void useMultiSkill();

	void teleportRandomPlayer();

	void cancelPiercingStrike();

	void cancelFireStorm();

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void lavaEruptionEvent(int32_t floorId);

	void spawnfloor(int32_t floor);

	void rndSpawn(int32_t npcId, int32_t count);

	void deleteAdds();

protected:
	void handleBackHome() override;

	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
