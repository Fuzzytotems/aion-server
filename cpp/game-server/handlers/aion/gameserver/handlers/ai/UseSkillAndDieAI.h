#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/commons/logging/Logger.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of an npc that casts its first npc skill once and leaves ("useSkillAndDie"): the skill's condition template gives the delay, the
 * despawn time after the cast and whether the npc can die meanwhile; an npc with a creator casts only while the creator lives.
 * <p>
 * Java: data/handlers/ai/UseSkillAndDieAI.java, @AIName("useSkillAndDie"). Java's `volatile boolean canDie` is a Field<bool>.
 */
class UseSkillAndDieAI : public NpcAI {
public:
	explicit UseSkillAndDieAI(Npc& owner) : NpcAI(owner) {}

	void handleSpawned() override;

	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

	void handleDied() override;

private:
	void scheduleSkill();

	runtime::Field<bool> canDie{true};

	static const commons::logging::Logger log;
};

} // namespace aion::gameserver::handlers::ai
