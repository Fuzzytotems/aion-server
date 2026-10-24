#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/commons/logging/Logger.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

/**
 * The boss of the Eternal Challenge ("custom_instance_boss"): it copies the challenger's appearance and stats and casts his skills as his
 * player model chooses them (D12, owner 2026-10-13).
 * <p>
 * Java: data/handlers/ai/instance/custom/eternalChallenge/CustomInstanceBossAI.java. C++: Java's plain fields are Fields (the skill rotation
 * runs on the scheduler).
 */
class CustomInstanceBossAI : public GeneralNpcAI {
private:
	runtime::Field<runtime::Ref<gameserver::custom::instance::neuralnetwork::PlayerModel>> model{};
	runtime::Field<runtime::FutureRef> skillTask{};
	runtime::Field<runtime::FutureRef> castTimeout{};
	runtime::Field<int32_t> previousSkill{};
	runtime::Field<int32_t> rank{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<int32_t>>> skillSet{};
	runtime::Field<bool> onlyAttack{};

public:
	explicit CustomInstanceBossAI(Npc& owner) : GeneralNpcAI(owner) {}

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;
	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;
	void handleCreatureDetected(Creature& creature) override;
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	/** C++-dev only, not in Java: tells the handler the boss hated the player (its DPS report's boss fight starts at the first time) */
	void handleAttack(runtime::Ptr<Creature> creature) override;
	void handleSpawned() override;
	void handleBackHome() override;
	void handleDied() override;
	void handleDespawned() override;

private:
	static const commons::logging::Logger& customInstanceLog();
	void checkSkillRotation();
	int32_t getPlayerSkill();
	void adaptAppearance(Player& player);
	void adaptStats(Player& player);
	void cancelTasks();
};

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
