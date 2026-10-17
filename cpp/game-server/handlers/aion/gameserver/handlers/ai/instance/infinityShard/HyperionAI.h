#pragma once

#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::infinityShard {

/**
 * The AI of Hyperion in Infinity Shard ("hyperion"): beyond 4 m (his and the target's bound radii off) he attacks ranged with earth magic,
 * 1.5 times his damage, beyond 10 m 4.5 times; his hand animations are random and his skills come without delay. His HP phases: 100 % three
 * Powerful Energy Blasts (21241), 80/75/67/55/45/30/17 % ancient tyrhunds (231103) around him, 75 % and 40 % a wave of guards walking from the
 * north and south (one of 231096-231101 each), 65/50/25/20 % 21253 then 21244, whose end calls a guard wave by the stage, 10 % 21246, whose end
 * starts the last phase (every 60 s from 35 s: four guards a side and four tyrhunds). Back home, despawned or dead the last phase stops.
 * <p>
 * Java: data/handlers/ai/instance/infinityShard/HyperionAI.java, @AIName("hyperion") (the marker is in the .cpp).
 * <p>
 * C++: the two spawn positions are built in the constructor from the owner's map id, as Java's field initializers do.
 *
 * @author Cheatkiller, Yeats, Estrayl
 */
class HyperionAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	// Java: = new HpPhases(100, 80, 75, 67, 65, 55, 50, 45, 40, 30, 25, 20, 17, 10)
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases =
		gameserver::ai::HpPhases::create(100, {80, 75, 67, 65, 55, 50, 45, 40, 30, 25, 20, 17, 10});
	const std::vector<int32_t> possibleSummons{231096, 231097, 231098, 231099, 231100, 231101}; // Java: Arrays.asList(...)
	const runtime::Ref<WorldPosition> northernSpawnPos;
	const runtime::Ref<WorldPosition> southernSpawnPos;
	runtime::Field<runtime::FutureRef> spawnTask{};
	runtime::Field<int8_t> stage{}; // Java: byte stage = 0
	runtime::Field<double> dist{};

public:
	explicit HyperionAI(Npc& owner);

	AttackTypeAnimation getAttackTypeAnimation(Creature& target) override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	ItemAttackType modifyAttackType(ItemAttackType type) override;

	AttackHandAnimation modifyAttackHandAnimation(AttackHandAnimation attackHandAnimation) override;

	void handleHpPhase(int32_t phaseHpPercent) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void scheduleLastPhase();

	void spawnAncientTyrhund(int32_t count);

	void spawnSummons(int32_t spawnCase);

	/** walkerId: std::nullopt for Java's null */
	void spawnWithWalker(int32_t npcId, Point3D& p, std::optional<std::string> walkerId);

	runtime::Ref<Point3D> getRndPos(WorldPosition& p, float distanceMod);

	void queuePowerfulEnergyBlast();

	void cancelSpawnTask();

public:
	int32_t modifyInitialSkillDelay(int32_t delay) override;

protected:
	void handleBackHome() override;

	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::infinityShard
