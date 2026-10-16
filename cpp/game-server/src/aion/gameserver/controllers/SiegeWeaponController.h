#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/summons/fwd.h"
#include "aion/gameserver/model/templates/npcskill/fwd.h"

namespace aion::gameserver::controllers {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). The controller part of siege weapon summons. The constructor reads the npc skill
 * templates (DataManager.NPC_SKILL_DATA must be published).
 *
 * @author xTz
 */
class SiegeWeaponController : public SummonController {
private:
	const model::templates::npcskill::NpcSkillTemplates* skills;

public:
	explicit SiegeWeaponController(int32_t npcId);
	~SiegeWeaponController() override;

	void release(model::summons::UnsummonType unsummonType) override;

	void restMode() override;

	void setUnkMode() override;

	void guardMode() override final;

	void attackMode(int32_t targetObjId) override;

	bool isValidTarget(model::gameobjects::Creature& target);

private:
	bool isBalaurBoss(model::gameobjects::Creature& creature);

public:
	void onDie(model::gameobjects::Creature& lastAttacker) override;

	const model::templates::npcskill::NpcSkillTemplates* getNpcSkillTemplates() const { return skills; }
};

} // namespace aion::gameserver::controllers
