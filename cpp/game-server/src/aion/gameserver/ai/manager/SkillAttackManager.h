#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/manager/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/skill/fwd.h"

namespace aion::gameserver::ai::manager {

/**
 * Chooses and casts the skills an NPC uses in a fight.
 * <p>
 * A static-only utility class (hub-headers.md §11.1). **M5b-1 stub (m5b-plan.md D4):** every body is an AION_PARTIAL, because the whole class
 * reaches the skill engine (SkillEngine, Skill, Effect, NpcSkillTemplateEntry), which M5b-2 ports. The bodies must **return** and not throw:
 * the spawn and attack paths have no catch, and AttackManager::chooseAttack reaches performAttack for an AttackIntention of SKILL_ATTACK.
 * GeneralNpcAI::chooseSkillAttack is the other half of D4 and answers false, so a correct M5b-1 server never asks for SKILL_ATTACK and never
 * reaches these bodies (the M5b gate asserts their hit count is 0, m5b-plan.md §6.1 §B).
 *
 * @author ATracer, Yeats
 */
class SkillAttackManager {
public:
	SkillAttackManager() = delete;

	static void performAttack(NpcAI& npcAI, int32_t delay);

	/** Java: protected (package access) */
	static void skillAction(NpcAI& npcAI);

	static bool cantUseSkill(model::skill::NpcSkillEntry& skill, model::gameobjects::Creature& owner);

	static void afterUseSkill(NpcAI& npcAI);

	/** @return the skill the npc should cast next, null when it has none ready */
	static runtime::Ptr<model::skill::NpcSkillEntry> chooseNextSkill(NpcAI& npcAI);
};

} // namespace aion::gameserver::ai::manager
