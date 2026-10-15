#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceBossAI.h"

#include <exception>
#include <memory>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/custom/instance/CustomInstanceService.h"
#include "aion/gameserver/custom/instance/RoahCustomInstanceHandler.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModel.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelController.h"
#include "aion/gameserver/custom/instance/neuralnetwork/PlayerModelEntry.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/NpcEquipmentList.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/condition/Conditions.h"
#include "aion/gameserver/skillengine/condition/DpCondition.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/stats/CalculationType.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge {

using gameserver::custom::instance::RoahCustomInstanceHandler;
using skillengine::effect::AbnormalState;

AION_AI(CustomInstanceBossAI, "custom_instance_boss");

/** Java: private static final Logger log = LoggerFactory.getLogger("CUSTOM_INSTANCE_LOG") */
const commons::logging::Logger& CustomInstanceBossAI::customInstanceLog() {
	static const auto logger = commons::logging::LoggerFactory::getLogger("CUSTOM_INSTANCE_LOG");
	return logger;
}

// Java CustomInstanceBossAI.java:64-67
float CustomInstanceBossAI::modifyDamage(Creature& attacker, float damage, runtime::Ptr<Effect> effect) {
	static_cast<void>(attacker);
	static_cast<void>(effect);
	return damage * 0.42f; // pseudo PvP reduce
}

// Java CustomInstanceBossAI.java:69-72
float CustomInstanceBossAI::modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) {
	static_cast<void>(effected);
	static_cast<void>(effect);
	return damage * 0.58f; // pseudo PvP reduce
}

// Java CustomInstanceBossAI.java:74-104
void CustomInstanceBossAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	previousSkill.set(-1);

	runtime::Ptr<world::WorldMapInstance> wmi = getPosition()->getWorldMapInstance();
	runtime::Ptr<RoahCustomInstanceHandler> handler = runtime::as<RoahCustomInstanceHandler>(wmi->getInstanceHandler());
	if (handler == nullptr)
		return;

	int32_t playerId = wmi->getRegisteredObjects().iterator().next();
	rank.set(gameserver::custom::instance::CustomInstanceService::getInstance().loadOrCreateRank(playerId).getRank());

	runtime::Ptr<Player> p = world::World::getInstance().getPlayer(playerId);
	if (p == nullptr) {
		customInstanceLog().error("[CI_ROAH] No player object found for player id: " + std::to_string(playerId)
				+ ". Either the player is offline or the central artifact was destroyed by something else.",
			std::runtime_error("new Exception()"));
		return;
	}

	if (p->getPlayerClass() == model::PlayerClass::RIDER) {
		onlyAttack.set(true);
	} else {
		onlyAttack.set(false);
		adaptAppearance(*p);
		adaptStats(*p);
		getOwner().setSeeState(CreatureSeeState::SEARCH2);
		// model player behavior
		skillSet.set(handler->getSkillSet());
		model.set(handler->getPlayerModel());
	}
}

// Java CustomInstanceBossAI.java:106-114
void CustomInstanceBossAI::handleBackHome() {
	GeneralNpcAI::handleBackHome();

	// prevent reset-abusing
	if (getOwner().getSkillCoolDowns() != nullptr)
		getOwner().getSkillCoolDowns()->clear();
	getOwner().getLifeStats()->setCurrentHpPercent(100);
}

// Java CustomInstanceBossAI.java:116-130
void CustomInstanceBossAI::handleCreatureDetected(Creature& creature) {
	GeneralNpcAI::handleCreatureDetected(creature);
	runtime::Ptr<world::WorldMapInstance> wmi = getPosition()->getWorldMapInstance();
	if (runtime::as<RoahCustomInstanceHandler>(wmi->getInstanceHandler()) == nullptr)
		return;

	if (PositionUtil::getDistance(getPosition()->getX(), getPosition()->getY(), getPosition()->getZ(), creature.getX(), creature.getY(),
			creature.getZ()) <= 45
		&& getPosition()->getWorldMapInstance()->isRegistered(creature.getObjectId())) {
		getOwner().getAggroList().addHate(creature, 100); // early aggro

		if (skillTask.get() == nullptr && !onlyAttack.get())
			skillTask.set(ThreadPoolManager::getInstance().scheduleAtFixedRate({this}, [this] { checkSkillRotation(); }, 200, 200));
	}
}

// Java CustomInstanceBossAI.java:132-155
void CustomInstanceBossAI::checkSkillRotation() {
	if (!getOwner().isCasting()) {
		int32_t skillID = getPlayerSkill();

		if (skillID != -1 && castTimeout.get() == nullptr) {
			runtime::Ref<skillengine::model::Skill> skill = skillengine::SkillEngine::getInstance().getSkill(getOwner(), skillID, 1, getTarget());
			skill->useSkill();

			// workaround for slide-cast bug:
			getOwner().getEffectController()->setAbnormal(AbnormalState::SANCTUARY);
			int64_t castDuration = static_cast<int64_t>(static_cast<float>(skill->getSkillTemplate()->getDuration())
				+ (0.3f * static_cast<float>(getOwner().getGameStats()->getAttackSpeed()->getCurrent())));
			if (skill->getSkillTemplate()->getCooldown() == 0) // item skills
				castDuration = 0;

			castTimeout.set(ThreadPoolManager::getInstance().schedule({this}, [this] {
				getOwner().getEffectController()->unsetAbnormal(AbnormalState::SANCTUARY);
				if (getOwner().getCastingSkill() != nullptr)
					getOwner().getCastingSkill()->cancelCast();

				// little break after timeout (for walking)
				castTimeout.set(ThreadPoolManager::getInstance().schedule({this}, [this] { castTimeout.set(nullptr); }, 1000));
			}, castDuration)); // after cast / instant skill: 300ms * attack speed
		}
	}
}

// Java CustomInstanceBossAI.java:157-218
int32_t CustomInstanceBossAI::getPlayerSkill() {
	runtime::Ptr<Creature> target = runtime::as<Creature>(getTarget());
	if (target == nullptr)
		return -1;

	// remove shock
	if ((getOwner().getEffectController()->isInAnyAbnormalState(AbnormalState::ANY_STUN)
			|| getOwner().getEffectController()->isAbnormalSet(AbnormalState::OPENAERIAL))
		&& getOwner().getSkillCoolDown(1968) <= commons::utils::currentTimeMillis())
		return 283;

	if (getOwner().getEffectController()->isInAnyAbnormalState(AbnormalState::CANT_ATTACK_STATE))
		return -1;

	// compute skill selection:
	runtime::Ptr<gameserver::custom::instance::neuralnetwork::PlayerModel> playerModel = model.get();
	if (playerModel != nullptr) {
		runtime::Ptr<runtime::RcArrayList<int32_t>> skills = skillSet.get();
		std::vector<int32_t> skillList;
		for (int32_t skillId : skills->snapshot())
			skillList.push_back(skillId);
		// assess game state:
		std::vector<double> inputArray =
			gameserver::custom::instance::neuralnetwork::PlayerModelEntry::create(getOwner(), -1, target)->toStateInputArray(skillList, previousSkill.get());

		std::vector<double> output = playerModel->getOutputEstimation(inputArray);
		for (size_t i = 0; i < output.size(); i++) {
			int32_t skillId = skills->get(static_cast<int32_t>(i));
			runtime::Ref<skillengine::model::Skill> skillI = skillengine::SkillEngine::getInstance().getSkill(getOwner(), skillId, 1, getTarget());
			if (skillI == nullptr) {
				customInstanceLog().warn("Detected a skill input with not existent template [skillId=" + std::to_string(skillId) + "].");
				output[i] = -1.0;
				continue;
			}

			int32_t cdID = -1; // item skills that have no cdID
			bool isDPskill = false;
			if (skillI->getSkillTemplate() != nullptr) {
				cdID = skillI->getSkillTemplate()->getCooldownId();
				if (skillI->getSkillTemplate()->getStartconditions() != nullptr) {
					for (const std::unique_ptr<skillengine::condition::Condition>& c : skillI->getSkillTemplate()->getStartconditions()->getConditions()) {
						if (dynamic_cast<const skillengine::condition::DpCondition*>(c.get()) != nullptr) {
							isDPskill = true;
							break;
						}
					}
				}
			}

			// rule out:
			if (isDPskill || !skillI->canUseSkill(skillengine::properties::Properties_CastState::CAST_START)
				|| (skillI->getSkillTemplate()->getType() == skillengine::model::SkillType::MAGICAL
					&& getOwner().getEffectController()->isAbnormalSet(AbnormalState::SILENCE))
				|| (skillI->getSkillTemplate()->getType() == skillengine::model::SkillType::PHYSICAL
					&& getOwner().getEffectController()->isAbnormalSet(AbnormalState::BIND))
				|| skillI->getSkillTemplate()->isCharge() || skillI->isPointSkill()
				|| (cdID != -1 && getOwner().getSkillCoolDown(cdID) > commons::utils::currentTimeMillis()))
				output[i] = -1.0; // -1 = minimum probability
		}

		int32_t skillIndex = gameserver::custom::instance::neuralnetwork::PlayerModelController::getMaxIndex(output);
		if (output.at(static_cast<size_t>(skillIndex)) == -1) // if no CDs available: auto-attack (Java: output.get of an empty list throws)
			return -1;

		return skills->get(skillIndex);
	}
	return -1;
}

// Java CustomInstanceBossAI.java:220-234
void CustomInstanceBossAI::onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) {
	GeneralNpcAI::onEndUseSkill(skillTemplate, skillLevel);
	previousSkill.set(skillTemplate->getSkillId());

	if (runtime::Ptr<runtime::Future> timeout = castTimeout.get()) {
		timeout->cancel(true);
		castTimeout.set(nullptr);
	}

	if (skillTemplate->getCooldown() == 0) // item skills: prevent spamming
		getOwner().setSkillCoolDown(skillTemplate->getCooldownId(), commons::utils::currentTimeMillis() + 60000);

	getOwner().getEffectController()->unsetAbnormal(AbnormalState::SANCTUARY);
}

// Java CustomInstanceBossAI.java:236-249
void CustomInstanceBossAI::adaptAppearance(Player& player) {
	auto v = std::make_unique<dataholders::loadingutils::adapters::NpcEquipmentList>();
	if (player.getEquipment().getMainHandWeapon() != nullptr) // weapons manually to exclude swap weapon slots
		v->items.push_back(player.getEquipment().getMainHandWeapon()->getItemSkinTemplate());
	if (player.getEquipment().getOffHandWeapon() != nullptr)
		v->items.push_back(player.getEquipment().getOffHandWeapon()->getItemSkinTemplate());
	for (runtime::Ptr<Item> i : player.getEquipment().getEquippedItems())
		if (i->getEquipmentType() == EquipType::ARMOR && i->getItemTemplate()->getItemSubType() != model::templates::item::enums::ItemSubType::SHIELD)
			v->items.push_back(i->getItemSkinTemplate());

	getOwner().overrideEquipmentList(std::move(v));
}

// Java CustomInstanceBossAI.java:251-305
void CustomInstanceBossAI::adaptStats(Player& player) {
	using model::stats::calc::functions::IStatFunction;
	using model::stats::calc::functions::RcStatFunction;
	using model::stats::calc::functions::StatSetFunction;
	using model::stats::container::StatEnum;
	using utils::stats::CalculationType;
	runtime::Ptr<model::stats::container::PlayerGameStats> pgs = player.getGameStats();
	std::vector<runtime::Ref<IStatFunction>> functions;
	auto set = [&functions](StatEnum stat, int32_t value) { functions.push_back(RcStatFunction<StatSetFunction>::create(stat, value)); };
	set(StatEnum::EARTH_RESISTANCE, pgs->getElementalDefenseFor(model::SkillElement::EARTH));
	set(StatEnum::FIRE_RESISTANCE, pgs->getElementalDefenseFor(model::SkillElement::FIRE));
	set(StatEnum::WATER_RESISTANCE, pgs->getElementalDefenseFor(model::SkillElement::WATER));
	set(StatEnum::WIND_RESISTANCE, pgs->getElementalDefenseFor(model::SkillElement::WIND));
	set(StatEnum::ABNORMAL_RESISTANCE_ALL, pgs->getAbnormalResistance()->getCurrent());
	set(StatEnum::ACCURACY, pgs->getAccuracy()->getCurrent());
	set(StatEnum::AGILITY, pgs->getAgility()->getCurrent());
	set(StatEnum::ATTACK_RANGE, 2500);
	set(StatEnum::BLOCK, pgs->getBlock()->getCurrent());
	set(StatEnum::EVASION, pgs->getEvasion()->getCurrent());
	set(StatEnum::HEALTH, pgs->getHealth()->getCurrent());
	set(StatEnum::KNOWLEDGE, pgs->getKnowledge()->getCurrent());
	set(StatEnum::MAGIC_SKILL_BOOST_RESIST, pgs->getMBResist()->getCurrent());
	set(StatEnum::MAGICAL_RESIST, pgs->getMResist()->getCurrent());
	set(StatEnum::MAGICAL_ACCURACY, pgs->getMAccuracy()->getCurrent());
	set(StatEnum::MAGICAL_CRITICAL, pgs->getMCritical()->getCurrent());
	set(StatEnum::MAIN_HAND_POWER, pgs->getMainHandPAttack({CalculationType::DISPLAY})->getCurrent());
	int32_t maxHP = pgs->getMaxHp()->getCurrent();
	maxHP += static_cast<int32_t>(static_cast<float>(maxHP * rank.get()) / 10.0f);
	if (onlyAttack.get())
		maxHP *= 10;
	set(StatEnum::MAXHP, maxHP);
	set(StatEnum::MAXMP, pgs->getMaxMp()->getCurrent());
	set(StatEnum::OFF_HAND_ACCURACY, pgs->getOffHandPAccuracy()->getCurrent());
	set(StatEnum::OFF_HAND_ATTACK_SPEED, pgs->getAttackSpeed()->getCurrent());
	set(StatEnum::OFF_HAND_CRITICAL, pgs->getOffHandPCritical()->getCurrent());
	set(StatEnum::OFF_HAND_POWER, pgs->getOffHandPAttack({CalculationType::DISPLAY})->getCurrent());
	set(StatEnum::PARRY, pgs->getParry()->getCurrent());
	set(StatEnum::PHYSICAL_ACCURACY, pgs->getMainHandPAccuracy()->getCurrent());
	set(StatEnum::PHYSICAL_DEFENSE, pgs->getPDef()->getCurrent());
	set(StatEnum::PHYSICAL_CRITICAL_RESIST, pgs->getPCR()->getCurrent());
	set(StatEnum::POWER, pgs->getPower()->getCurrent());
	set(StatEnum::SPEED, pgs->getMovementSpeed()->getCurrent());
	set(StatEnum::WILL, pgs->getWill()->getCurrent());
	set(StatEnum::ATTACK_SPEED, pgs->getAttackSpeed()->getCurrent());
	// Work-around for not considered dual wield stats for NPCs
	int32_t pAtk = pgs->getMainHandPAttack({CalculationType::DISPLAY})->getCurrent();
	if (player.getEquipment().getOffHandWeapon() != nullptr)
		pAtk += pgs->getOffHandPAttack({CalculationType::DISPLAY})->getCurrent() / 2;
	set(StatEnum::PHYSICAL_ATTACK, pAtk);

	if (model::isPhysicalClass(player.getPlayerClass())) {
		set(StatEnum::PHYSICAL_CRITICAL, pgs->getMainHandPCritical()->getCurrent());
	} else {
		set(StatEnum::MAGICAL_CRITICAL, pgs->getMCritical()->getCurrent());
		set(StatEnum::MAGICAL_ATTACK, pgs->getMainHandMAttack(std::initializer_list<CalculationType>{})->getCurrent());
		set(StatEnum::BOOST_MAGICAL_SKILL, pgs->getMBoost()->getCurrent());
	}

	std::vector<runtime::Ptr<IStatFunction>> borrowed(functions.begin(), functions.end());
	getOwner().getGameStats()->addEffect(nullptr, borrowed);
	getOwner().getLifeStats()->setCurrentHp(getOwner().getLifeStats()->getMaxHp());
}

// Java CustomInstanceBossAI.java:307-312
void CustomInstanceBossAI::cancelTasks() {
	runtime::Ptr<runtime::Future> task = skillTask.get();
	if (task != nullptr && !task->isCancelled())
		task->cancel(true);
	runtime::Ptr<runtime::Future> timeout = castTimeout.get();
	if (timeout != nullptr && !timeout->isCancelled())
		timeout->cancel(true);
}

// Java CustomInstanceBossAI.java:314-326
void CustomInstanceBossAI::handleDied() {
	cancelTasks();
	if (model.get() == nullptr) {
		if (onlyAttack.get())
			PacketSendUtility::broadcastToMap(getOwner(), SM_MESSAGE(getOwner(), "Remarkable... ", ChatType::BRIGHT_YELLOW_CENTER));
		else
			PacketSendUtility::broadcastToMap(getOwner(), SM_MESSAGE(getOwner(), "Remarkable ... I will ... remember you.", ChatType::BRIGHT_YELLOW_CENTER));
	} else {
		PacketSendUtility::broadcastToMap(getOwner(), SM_MESSAGE(getOwner(), "One day ... I will ... suppress you!", ChatType::BRIGHT_YELLOW_CENTER));
	}
	GeneralNpcAI::handleDied();
}

// Java CustomInstanceBossAI.java:328-332
void CustomInstanceBossAI::handleDespawned() {
	cancelTasks();
	GeneralNpcAI::handleDespawned();
}

} // namespace aion::gameserver::handlers::ai::instance::custom::eternalChallenge
