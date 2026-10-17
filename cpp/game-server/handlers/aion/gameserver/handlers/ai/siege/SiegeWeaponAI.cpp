#include "aion/gameserver/handlers/ai/siege/SiegeWeaponAI.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AILogger.h"
#include "aion/gameserver/ai/handler/FollowEventHandler.h"
#include "aion/gameserver/controllers/SiegeWeaponController.h"
#include "aion/gameserver/controllers/movement/SummonMoveController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplate.h"
#include "aion/gameserver/model/templates/npcskill/NpcSkillTemplates.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(SiegeWeaponAI, "siege_weapon");

// Java SiegeWeaponAI.java:36-43 (getFirst() on an empty list is Java's NoSuchElementException, getSkillTemplate(skill).getDuration() on null
// its NullPointerException)
void SiegeWeaponAI::handleSpawned() {
	setStateIfNot(AIState::IDLE);
	services::summons::SummonsService::doMode(model::summons::SummonMode::GUARD, getOwner());
	const std::vector<model::templates::npcskill::NpcSkillTemplate>& npcSkills = getNpcSkillTemplates().getNpcSkills();
	if (npcSkills.empty())
		throw runtime::NoSuchElementException("SiegeWeaponAI: no npc skill");
	const model::templates::npcskill::NpcSkillTemplate& skillTemplate = npcSkills.front();
	skill = skillTemplate.getSkillId();
	skillLvl = skillTemplate.getSkillLevel();
	const skillengine::model::SkillTemplate* template_ = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skill.get());
	if (template_ == nullptr)
		throw runtime::NullPointerException("SiegeWeaponAI: skill template " + std::to_string(skill.get()));
	duration = template_->getDuration();
}

// Java SiegeWeaponAI.java:45-48
void SiegeWeaponAI::handleFollowMe(Creature& creature) {
	static_cast<void>(creature);
	setStateIfNot(AIState::FOLLOWING);
}

// Java SiegeWeaponAI.java:50-54
void SiegeWeaponAI::handleStopFollowMe(Creature& creature) {
	static_cast<void>(creature);
	setStateIfNot(AIState::IDLE);
	getOwner().getMoveController()->abortMove();
}

// Java SiegeWeaponAI.java:56-59
void SiegeWeaponAI::handleTargetTooFar() {
	getOwner().getMoveController()->moveToDestination();
}

// Java SiegeWeaponAI.java:61-65
void SiegeWeaponAI::handleMoveArrived() {
	getOwner().getController().onMove();
	getOwner().getMoveController()->abortMove();
}

// Java SiegeWeaponAI.java:67-71
void SiegeWeaponAI::handleMoveValidate() {
	getOwner().getController().onMove();
	getOwner().getMoveController()->moveToTargetObject();
}

// Java SiegeWeaponAI.java:73-75 (Java's cast (SiegeWeaponController) getOwner().getController())
const model::templates::npcskill::NpcSkillTemplates& SiegeWeaponAI::getNpcSkillTemplates() {
	auto* controller = dynamic_cast<controllers::SiegeWeaponController*>(&getOwner().getController());
	if (controller == nullptr)
		throw runtime::ClassCastException("SiegeWeaponAI: the controller is not a SiegeWeaponController");
	const model::templates::npcskill::NpcSkillTemplates* templates = controller->getNpcSkillTemplates();
	if (templates == nullptr) // Java: getNpcSkills() on the null templates
		throw runtime::NullPointerException("SiegeWeaponAI: no npc skill templates");
	return *templates;
}

// Java SiegeWeaponAI.java:77-88
void SiegeWeaponAI::handleAttack(runtime::Ptr<Creature> creature) {
	if (creature == nullptr || getOwner().getMode() != model::summons::SummonMode::ATTACK)
		return;
	auto* controller = dynamic_cast<controllers::SiegeWeaponController*>(&getOwner().getController());
	if (controller != nullptr && controller->isValidTarget(*creature)) {
		if (commons::utils::currentTimeMillis() - lastAttackTime.get() > duration.get() + 2000) {
			lastAttackTime = commons::utils::currentTimeMillis();
			getOwner().getController().useSkill(skill.get(), skillLvl.get());
		}
	}
}

// Java SiegeWeaponAI.java:90-99
bool SiegeWeaponAI::isDestinationReached() {
	AIState state = getState();
	if (state == AIState::FOLLOWING) {
		return FollowEventHandler::isInRange(*this, getOwner().getTarget());
	} else {
		AILogger::info(*this, "[siege_weapon] calling destinationReached with unhandled state: " + std::string(xml::enumName(state)));
	}
	return true;
}

// Java SiegeWeaponAI.java:101-107
bool SiegeWeaponAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return AITemplate::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::siege
