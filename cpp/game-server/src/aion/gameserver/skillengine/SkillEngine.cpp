#include "aion/gameserver/skillengine/SkillEngine.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine {

// Java: LoggerFactory.getLogger(SkillEngine.class) inside checkAndGetSkillTemplate (hub-headers.md §11.3)
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.skillengine.SkillEngine");

SkillEngine::SkillEngine() = default;

runtime::Ref<model::Skill> SkillEngine::getSkillFor(gameserver::model::gameobjects::player::Player& player, int32_t skillId,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget) {
	AION_UNPORTED();
}

runtime::Ref<model::Skill> SkillEngine::getSkillFor(gameserver::model::gameobjects::player::Player& player, const model::SkillTemplate* template_,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget) {
	AION_UNPORTED();
}

runtime::Ref<model::Skill> SkillEngine::getSkillFor(gameserver::model::gameobjects::player::Player& player, const model::SkillTemplate* template_,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget, int32_t skillLevel) {
	AION_UNPORTED();
}

runtime::Ref<model::Skill> SkillEngine::getSkill(gameserver::model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget) {
	AION_UNPORTED();
}

runtime::Ref<model::Skill> SkillEngine::getSkill(gameserver::model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget, const gameserver::model::templates::item::ItemTemplate* itemTemplate) {
	AION_UNPORTED();
}

runtime::Ref<model::ChargeSkill> SkillEngine::getChargeSkill(gameserver::model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel,
	int32_t motionId, model::Skill& startSkill) {
	AION_UNPORTED();
}

runtime::Ref<model::PenaltySkill> SkillEngine::getPenaltySkill(gameserver::model::gameobjects::Creature& effector, int32_t skillId, int32_t skillLevel) {
	AION_UNPORTED();
}

SkillEngine& SkillEngine::getInstance() {
	static SkillEngine instance;
	return instance;
}

runtime::Ref<model::Effect> SkillEngine::applyEffectDirectly(int32_t skillId, gameserver::model::gameobjects::Creature& effector,
	gameserver::model::gameobjects::Creature& effected) {
	AION_UNPORTED();
}

runtime::Ref<model::Effect> SkillEngine::applyEffectDirectly(int32_t skillId, gameserver::model::gameobjects::Creature& effector,
	gameserver::model::gameobjects::Creature& effected, std::optional<int32_t> duration, const model::Effect_ForceType* forceType) {
	AION_UNPORTED();
}

runtime::Ref<model::Effect> SkillEngine::applyEffectDirectly(int32_t skillId, int32_t lvl, gameserver::model::gameobjects::Creature& effector,
	gameserver::model::gameobjects::Creature& effected, std::optional<int32_t> duration, const model::Effect_ForceType* forceType) {
	AION_UNPORTED();
}

runtime::Ref<model::Effect> SkillEngine::applyEffectDirectly(const model::SkillTemplate* skillTemplate, int32_t skillLevel,
	gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& effected) {
	// Java: return applyEffect(effector, effected, skillTemplate, skillLevel, null, ForceType.DEFAULT) - new Effect(...), initialize(),
	// applyEffect(). Effect application is M5b work (m5a-plan.md O-09); the enter-world passive skills (PlayerEnterWorldService
	// .activatePassiveSkillEffects) ignore the returned effect.
	AION_PARTIAL("passive skill effects are not applied yet (M5a O-09)");
	return {};
}

std::vector<runtime::Ptr<gameserver::model::gameobjects::Creature>> SkillEngine::applyEffectsDirectly(int32_t skillId,
	gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& firstTarget, float x, float y, float z) {
	AION_UNPORTED();
}

runtime::Ref<model::Effect> SkillEngine::applyEffect(int32_t skillId, gameserver::model::gameobjects::Creature& effector,
	gameserver::model::gameobjects::Creature& effected) {
	AION_UNPORTED();
}

runtime::Ref<model::Effect> SkillEngine::applyEffect(gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& effected,
	const model::SkillTemplate* skillTemplate, int32_t lvl, std::optional<int32_t> duration, const model::Effect_ForceType* forceType) {
	AION_UNPORTED();
}

const model::SkillTemplate* SkillEngine::checkAndGetSkillTemplate(int32_t skillId) {
	AION_UNPORTED();
}

runtime::Ref<model::Effect> SkillEngine::createCriticalProcEffect(gameserver::model::gameobjects::player::Player& attacker,
	gameserver::model::gameobjects::Creature& target, int32_t skillId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine
