#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::worlds::heiron {

/**
 * The AI of Bollvig in Heiron ("bollvig"): his spawn deletes 204655. At 75 and 50 % HP he casts Sleep of Death (17861) and calls two 280802
 * and two 280803 10 m around (1001, 2828), at 25 % he goes straight on: between 50 and 25 % Nerve Absorption (18034) with a 280804 after
 * 10 s, at 25 % or less Blood Cell Destruction (18037); 31 s later Charming Attraction (17899), 5 s later Curse of Soul (18025) and the round
 * again above 25 %, Mortal Cutting (18027) and Charming Attraction again 11 s later at 25 % or less. Back home, despawned or dead he cancels
 * a task, and despawned or dead he deletes his summons and spawns 204655 again when neither it nor a living 212314 is there.
 * <p>
 * Java: data/handlers/ai/worlds/heiron/BollvigAI.java, @AIName("bollvig") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class BollvigAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75, {50, 25}); // Java: = new HpPhases(75, 50, 25)
	runtime::Field<runtime::FutureRef> firstTask{};
	runtime::Field<runtime::FutureRef> secondTask{};
	runtime::Field<runtime::FutureRef> thirdTask{};
	runtime::Field<runtime::FutureRef> lastTask{};

public:
	explicit BollvigAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void useFirstSkillTree();

	void firstSkill();

	void skillThree();

	void cancelTask();

	void rndSpawnInRange(int32_t npcId);

	void useSkill(int32_t skillId);

protected:
	void handleBackHome() override;

	void handleDespawned() override;

	void handleDied() override;

private:
	void deleteSummons(int32_t npcId);

	bool checkNpc();
};

} // namespace aion::gameserver::handlers::ai::worlds::heiron
