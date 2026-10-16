#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

#include <cstdint>

#include "aion/gameserver/ai/AITemplate.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::siege {

/**
 * A siege weapon, a Summon ("siege_weapon"): it spawns in GUARD mode with the first skill of its controller's npc skills; it follows its
 * master on request and, in ATTACK mode, uses that skill on a valid target once per skill duration + 2 s; it is immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/siege/SiegeWeaponAI.java, @AIName("siege_weapon") (the marker is in the .cpp). Its owner type is Summon (Java
 * AITemplate&lt;Summon&gt;).
 *
 * @author xTz
 */
class SiegeWeaponAI : public AITemplate<model::gameobjects::Summon> {
private:
	runtime::Field<int64_t> lastAttackTime{};
	runtime::Field<int32_t> skill{};
	runtime::Field<int32_t> skillLvl{};
	runtime::Field<int32_t> duration{};

public:
	explicit SiegeWeaponAI(model::gameobjects::Summon& owner) : AITemplate(owner) {}

protected:
	void handleSpawned() override;

	void handleFollowMe(Creature& creature) override;

	void handleStopFollowMe(Creature& creature) override;

	void handleTargetTooFar() override;

	void handleMoveArrived() override;

	void handleMoveValidate() override;

private:
	const model::templates::npcskill::NpcSkillTemplates& getNpcSkillTemplates();

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	bool isDestinationReached() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::siege
