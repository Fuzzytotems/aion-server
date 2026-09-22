#include "aion/gameserver/ai/manager/SkillAttackManager.h"

#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/event/AIEventType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/skill/NpcSkillEntry.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::ai::manager {

// m5b-plan.md D4. Every body of SkillAttackManager.java reaches the skill engine: performAttack schedules skillAction, skillAction reads
// SkillTemplate.getProperties() and calls Creature.useSkill, chooseNextSkill walks NpcSkillList with NpcSkillEntry.isReady/conditionReady and
// NpcGameStats.canUseNextSkill. SkillEngine, Skill, Effect and NpcSkillTemplateEntry are M5b-2 (m5b-plan.md O-01/O-03), so the bodies are
// AION_PARTIAL and return the neutral answer instead of throwing: AttackManager::chooseAttack calls performAttack from the attack path, which
// has no catch (m5a-plan.md §7).

void SkillAttackManager::performAttack(NpcAI& npcAI, int32_t delay) {
	AION_PARTIAL("npc skill attacks are not performed yet (M5b-2); GeneralNpcAI::chooseSkillAttack answers false, so this is unreachable");
}

void SkillAttackManager::skillAction(NpcAI& npcAI) {
	AION_PARTIAL("npc skill casting is not ported yet (M5b-2)");
}

bool SkillAttackManager::cantUseSkill(model::skill::NpcSkillEntry& skill, model::gameobjects::Creature& owner) {
	// Java reads EffectController.isInAnyAbnormalState / isAbnormalSet and SkillTemplate.getType (SkillAttackManager.java:105-110), all M5b-2.
	AION_PARTIAL("the npc skill usability checks are not ported yet (M5b-2)");
	return true; // Java's refusing answer: a caller that reaches it skips the skill instead of casting an unchecked one
}

void SkillAttackManager::afterUseSkill(NpcAI& npcAI) {
	// The two statements of SkillAttackManager.java:113-114 need no skill engine, so they are ported: a caller that got this far has left the
	// CAST sub state behind and must resume attacking, and leaving the sub state set would freeze the npc.
	npcAI.setSubStateIfNot(AISubState::NONE);
	npcAI.onGeneralEvent(event::AIEventType::ATTACK_COMPLETE);
}

runtime::Ptr<model::skill::NpcSkillEntry> SkillAttackManager::chooseNextSkill(NpcAI& npcAI) {
	AION_PARTIAL("npc skill selection is not ported yet (M5b-2)");
	return nullptr; // Java's "no skill ready" answer, which makes GeneralNpcAI.chooseAttackIntention choose SIMPLE_ATTACK
}

} // namespace aion::gameserver::ai::manager
