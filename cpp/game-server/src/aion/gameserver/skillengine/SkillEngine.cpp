#include "aion/gameserver/skillengine/SkillEngine.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/skillengine/effect/EffectType.h"
#include "aion/gameserver/skillengine/model/ActivationAttribute.h"
#include "aion/gameserver/skillengine/model/ChargeSkill.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/PenaltySkill.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/model/SkillType.h"
#include "aion/gameserver/skillengine/properties/Properties.h"

namespace aion::gameserver::skillengine {

using dataholders::DataManager;
using effect::EffectType;
using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::VisibleObject;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::templates::item::enums::ItemGroup;
using model::ActivationAttribute;
using model::Effect;
using model::Effect_ForceType;
using model::SkillTemplate;
using runtime::Ptr;
using runtime::Ref;

// Java: LoggerFactory.getLogger(SkillEngine.class) inside checkAndGetSkillTemplate (hub-headers.md §11.3)
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.skillengine.SkillEngine");

SkillEngine::SkillEngine() = default;

runtime::Ref<model::Skill> SkillEngine::getSkillFor(gameserver::model::gameobjects::player::Player& player, int32_t skillId,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget) {
	const SkillTemplate* template_ = DataManager::SKILL_DATA->getSkillTemplate(skillId);

	if (template_ == nullptr)
		return {};

	return getSkillFor(player, template_, firstTarget);
}

runtime::Ref<model::Skill> SkillEngine::getSkillFor(gameserver::model::gameobjects::player::Player& player, const model::SkillTemplate* template_,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget) {
	// player doesn't have such skill and ist not provoked
	if (template_->getActivationAttribute() != ActivationAttribute::PROVOKED) {
		if (!player.getSkillList()->isSkillPresent(template_->getSkillId()))
			return {};
	}

	Ptr<Creature> target = runtime::as<Creature>(firstTarget);

	return model::Skill::create(template_, player, target);
}

runtime::Ref<model::Skill> SkillEngine::getSkillFor(gameserver::model::gameobjects::player::Player& player, const model::SkillTemplate* template_,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget, int32_t skillLevel) {
	Ptr<Creature> target = runtime::as<Creature>(firstTarget);

	return model::Skill::create(template_, player, target, skillLevel);
}

runtime::Ref<model::Skill> SkillEngine::getSkill(gameserver::model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget) {
	return getSkill(creature, skillId, skillLevel, firstTarget, nullptr);
}

runtime::Ref<model::Skill> SkillEngine::getSkill(gameserver::model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel,
	runtime::Ptr<gameserver::model::gameobjects::VisibleObject> firstTarget, const gameserver::model::templates::item::ItemTemplate* itemTemplate) {
	const SkillTemplate* template_ = DataManager::SKILL_DATA->getSkillTemplate(skillId);

	if (template_ == nullptr)
		return {};

	Ptr<Creature> target = runtime::as<Creature>(firstTarget);
	return model::Skill::create(template_, creature, skillLevel, target, itemTemplate);
}

runtime::Ref<model::ChargeSkill> SkillEngine::getChargeSkill(gameserver::model::gameobjects::Creature& creature, int32_t skillId, int32_t skillLevel,
	int32_t motionId, model::Skill& startSkill) {
	const SkillTemplate* template_ = DataManager::SKILL_DATA->getSkillTemplate(skillId);
	if (template_ == nullptr)
		return {};
	return model::ChargeSkill::create(template_, creature, skillLevel, motionId, startSkill);
}

runtime::Ref<model::PenaltySkill> SkillEngine::getPenaltySkill(gameserver::model::gameobjects::Creature& effector, int32_t skillId, int32_t skillLevel) {
	const SkillTemplate* template_ = DataManager::SKILL_DATA->getSkillTemplate(skillId);
	if (template_ == nullptr)
		return {};

	return model::PenaltySkill::create(template_, effector, skillLevel);
}

SkillEngine& SkillEngine::getInstance() {
	static SkillEngine instance;
	return instance;
}

runtime::Ref<model::Effect> SkillEngine::applyEffectDirectly(int32_t skillId, gameserver::model::gameobjects::Creature& effector,
	gameserver::model::gameobjects::Creature& effected) {
	const SkillTemplate* skillTemplate = checkAndGetSkillTemplate(skillId);
	return skillTemplate == nullptr ? Ref<Effect>()
									: applyEffect(effector, effected, skillTemplate, skillTemplate->getLvl(), std::nullopt, Effect_ForceType::DEFAULT);
}

runtime::Ref<model::Effect> SkillEngine::applyEffectDirectly(int32_t skillId, gameserver::model::gameobjects::Creature& effector,
	gameserver::model::gameobjects::Creature& effected, std::optional<int32_t> duration, const model::Effect_ForceType* forceType) {
	const SkillTemplate* skillTemplate = checkAndGetSkillTemplate(skillId);
	return skillTemplate == nullptr ? Ref<Effect>() : applyEffect(effector, effected, skillTemplate, skillTemplate->getLvl(), duration, forceType);
}

runtime::Ref<model::Effect> SkillEngine::applyEffectDirectly(int32_t skillId, int32_t lvl, gameserver::model::gameobjects::Creature& effector,
	gameserver::model::gameobjects::Creature& effected, std::optional<int32_t> duration, const model::Effect_ForceType* forceType) {
	const SkillTemplate* skillTemplate = checkAndGetSkillTemplate(skillId);
	return skillTemplate == nullptr ? Ref<Effect>() : applyEffect(effector, effected, skillTemplate, lvl, duration, forceType);
}

runtime::Ref<model::Effect> SkillEngine::applyEffectDirectly(const model::SkillTemplate* skillTemplate, int32_t skillLevel,
	gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& effected) {
	// Java: return applyEffect(effector, effected, skillTemplate, skillLevel, null, ForceType.DEFAULT). Part 2 of M5b-2 ported that path
	// (m5b2-plan.md S-01, D2), but every enter-world passive reaches BufEffect::applyEffect and the mastery effects, which part 3 ports; until
	// then the path would throw UnportedException before setActivePlayer, so O-09 stays held back behind its partial.
	AION_PARTIAL("passive skill effects are not applied yet (M5a O-09)");
	return {};
}

std::vector<runtime::Ptr<gameserver::model::gameobjects::Creature>> SkillEngine::applyEffectsDirectly(int32_t skillId,
	gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& firstTarget, float x, float y, float z) {
	const SkillTemplate* skillTemplate = DataManager::SKILL_DATA->getSkillTemplate(skillId);
	if (skillTemplate == nullptr) // Java: skillTemplate.getProperties() dereferences the unchecked template
		throw runtime::NullPointerException("Cannot invoke \"SkillTemplate.getProperties()\" because \"skillTemplate\" is null");
	const properties::Properties* properties = skillTemplate->getProperties();
	// Java: a local ArrayList that Properties.validateEffectedList filters in place; the shim type is the one its signature takes (Properties.h)
	runtime::ArrayList<Ref<Creature>> targets;
	targets.add(Ref<Creature>(firstTarget));
	if (properties != nullptr) // add valid targets in range
		properties->validateEffectedList(targets, firstTarget, effector, skillTemplate, x, y, z);
	std::vector<Ptr<Creature>> result = targets.snapshot();
	for (Ptr<Creature> target : result)
		applyEffect(effector, *target, skillTemplate, skillTemplate->getLvl(), std::nullopt, Effect_ForceType::DEFAULT);
	return result;
}

runtime::Ref<model::Effect> SkillEngine::applyEffect(int32_t skillId, gameserver::model::gameobjects::Creature& effector,
	gameserver::model::gameobjects::Creature& effected) {
	const SkillTemplate* skillTemplate = checkAndGetSkillTemplate(skillId);
	return skillTemplate == nullptr ? Ref<Effect>() : applyEffect(effector, effected, skillTemplate, skillTemplate->getLvl(), std::nullopt, nullptr);
}

runtime::Ref<model::Effect> SkillEngine::applyEffect(gameserver::model::gameobjects::Creature& effector, gameserver::model::gameobjects::Creature& effected,
	const model::SkillTemplate* skillTemplate, int32_t lvl, std::optional<int32_t> duration, const model::Effect_ForceType* forceType) {
	Ref<Effect> ef = Effect::create(effector, effected, skillTemplate, lvl, duration, forceType);
	ef->initialize();
	ef->applyEffect();
	return ef;
}

const model::SkillTemplate* SkillEngine::checkAndGetSkillTemplate(int32_t skillId) {
	const SkillTemplate* skillTemplate = DataManager::SKILL_DATA->getSkillTemplate(skillId);
	if (skillTemplate == nullptr) {
		// Java: warn(message, new IllegalArgumentException()) - the exception only carries the stack trace into the log
		log.warn("Could not apply effect, invalid skill id " + std::to_string(skillId), commons::utils::IllegalArgumentException(""));
		return nullptr;
	}
	return skillTemplate;
}

runtime::Ref<model::Effect> SkillEngine::createCriticalProcEffect(gameserver::model::gameobjects::player::Player& attacker,
	gameserver::model::gameobjects::Creature& target, int32_t skillId) {
	// m5b-plan.md D14 closed (m5b2-plan.md S-01, D7): the body is SkillEngine.java:193-225 in Java's order again, with the isUnderNormalShield
	// guard first and the skillId != 0 filter before the weapon-group switch
	if (target.getEffectController()->isUnderNormalShield())
		return {};
	if (skillId != 0) {
		const SkillTemplate* skillTemplate = DataManager::SKILL_DATA->getSkillTemplate(skillId);
		if (skillTemplate == nullptr) // Java: skillTemplate.getType() dereferences the unchecked template
			throw runtime::NullPointerException("Cannot invoke \"SkillTemplate.getType()\" because \"skillTemplate\" is null");
		if (skillTemplate->getType() == model::SkillType::MAGICAL) // magical skills do not stun
			return {};
		if (skillTemplate->hasAnyEffect(true, {EffectType::PULLED, EffectType::STUMBLE, EffectType::STAGGER, EffectType::STUN, EffectType::BACKDASH,
				EffectType::DASH, EffectType::MOVEBEHIND, EffectType::RANDOMMOVELOC, EffectType::RECALLINSTANT})
			|| !skillTemplate->hasAnyEffect({EffectType::SKILLATKDRAININSTANT, EffectType::SKILLATTACKINSTANT}))
			return {};
	}

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

	const SkillTemplate* skillTemplate = checkAndGetSkillTemplate(id);
	if (skillTemplate != nullptr) {
		Ref<Effect> ef = Effect::create(attacker, target, skillTemplate, skillTemplate->getLvl(), std::nullopt, nullptr, true, nullptr);
		ef->initialize();
		return ef;
	}
	return {};
}

} // namespace aion::gameserver::skillengine
