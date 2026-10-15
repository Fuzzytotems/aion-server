#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::ai::instance::idgelDome {

/**
 * The AI of the unstable Ide energy in Idgel Dome ("unstable_id_energy"): no damage; skill 21559 after 10 to 30 s, again after each use.
 * <p>
 * Java: data/handlers/ai/instance/idgelDome/UnstableIdeEnergyAI.java, @AIName("unstable_id_energy") (the marker is in the .cpp).
 *
 * @author Ritsu, Estrayl
 */
class UnstableIdeEnergyAI : public NpcAI {
private:
	runtime::Field<runtime::FutureRef> skillTask{};

public:
	explicit UnstableIdeEnergyAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	float modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) override;

private:
	void scheduleSkill(int32_t delay);

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::idgelDome
