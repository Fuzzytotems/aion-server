#include "aion/gameserver/skillengine/SkillEngine.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::skillengine {

using gameserver::model::templates::item::enums::ItemGroup;

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
	// Reordered against SkillEngine.java:193-225 (m5b-plan.md D14, docs/deviations/P5-02.md). Java runs, in order:
	//   (1) `if (target.getEffectController().isUnderNormalShield()) return null;`                        (SkillEngine.java:194-195)
	//   (2) the `skillId != 0` filter over the skill template's type and effect types                     (:196-204)
	//   (3) the main-hand weapon-group switch below                                                       (:206-213)
	//   (4) `if (id == 0) return null;` and otherwise `new Effect(...).initialize()`                      (:215-224)
	// Steps (1) and (2) are skipped and the switch is evaluated first, because the switch alone decides whether Java can return anything but
	// null: every weapon group other than POLEARM/STAFF/GREATSWORD/BOW leaves `id` 0, and for those the answer is null whatever (1) and (2)
	// would have decided. So the `id == 0` arm is Java-exact; only the `id != 0` arm is short of the effect engine, and it is marked partial.
	// Skipping (1) can only differ while a normal shield effect is up, and EffectController::isUnderNormalShield is itself AION_UNPORTED
	// (EffectController.cpp:122-124) - no effect exists before M5b-2. Skipping (2) can only differ for `skillId != 0`, which no caller passes
	// yet (the only call site is CreatureController.cpp:375, Java CreatureController.java:341-345, with a literal 0); it also drops Java's NPE
	// for a skillId whose template is missing (SkillEngine.java:198 dereferences an unchecked null).
	int32_t id = 0;
	std::optional<ItemGroup> mainHandWeaponType = attacker.getEquipment().getMainHandWeaponType();
	if (mainHandWeaponType.has_value()) {
		switch (*mainHandWeaponType) {
			case ItemGroup::POLEARM:
			case ItemGroup::STAFF:
			case ItemGroup::GREATSWORD:
				id = 8218; // stumble
				break;
			case ItemGroup::BOW:
				id = 8217; // stun
				break;
			default: // Java: an arrow switch without a default arm leaves id at 0
				break;
		}
	}

	if (id == 0)
		return {};

	// Java: checkAndGetSkillTemplate(id), then `new Effect(attacker, target, skillTemplate, skillTemplate.getLvl(), null, null, true, null)` and
	// initialize(). Effect is 83 of 95 bodies unported (m5b-plan.md O-01); M5b-2 closes this arm together with the isUnderNormalShield guard.
	AION_PARTIAL("critical proc stumble/stun needs the effect engine (M5b-2)");
	return {};
}

} // namespace aion::gameserver::skillengine
