#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionMountableTankAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/panesterra/ahserion/AhserionRaid.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionMountableTankAI, "ahserion_mountable_tank");

using services::panesterra::ahserion::PanesterraFaction;

// Java AhserionMountableTankAI.java:27-39
void AhserionMountableTankAI::handleSpawned() {
	ActionItemNpcAI::handleSpawned();
	switch (getNpcId()) {
		case 277233:
		case 277238:
			ownerFaction = PanesterraFaction::BELUS;
			break;
		case 277234:
		case 277239:
			ownerFaction = PanesterraFaction::ASPIDA;
			break;
		case 277235:
		case 277240:
			ownerFaction = PanesterraFaction::ATANATOS;
			break;
		case 277236:
		case 277241:
			ownerFaction = PanesterraFaction::DISILLON;
			break;
		default:
			ownerFaction = PanesterraFaction::BALAUR;
			break;
	}
	getOwner().getController().addTask(model::TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({this}, [this] {
		getOwner().getController().deleteIfAliveOrCancelRespawn();
	}, 12, runtime::TimeUnit::MINUTES));
}

// Java AhserionMountableTankAI.java:41-49
void AhserionMountableTankAI::handleUseItemFinish(Player& player) {
	if (canUseTank(player) && canUse.compareAndSet(true, false)) {
		int32_t skillId = getSkillId();
		if (skillId != 0) {
			runtime::Ref<skillengine::model::Skill> skill = SkillEngine::getInstance().getSkill(getOwner(), skillId, 65, runtime::Ptr<VisibleObject>(player));
			if (skill == nullptr) // Java: getSkill(...).useNoAnimationSkill() on null
				throw runtime::NullPointerException("AhserionMountableTankAI: skill " + std::to_string(skillId));
			skill->useNoAnimationSkill();
		}
		AIActions::deleteOwner(*this);
	}
}

// Java AhserionMountableTankAI.java:51-57
bool AhserionMountableTankAI::canUseTank(Player& player) {
	if (AhserionRaid::getInstance().isStarted()) {
		runtime::Ptr<services::panesterra::ahserion::PanesterraTeam> team = PanesterraService::getInstance().getTeam(player);
		return team != nullptr && team->getFaction() == ownerFaction.get();
	}
	return false;
}

// Java AhserionMountableTankAI.java:59-73
int32_t AhserionMountableTankAI::getSkillId() {
	switch (getNpcId()) {
		// Board the Chariot
		case 277233:
			return 21582; // Belus
		case 277234:
			return 21589; // Aspida
		case 277235:
			return 21590; // Atanatos
		case 277236:
			return 21591; // Disillon
		// Board the Ignus Engine
		case 277238:
			return 21579; // Belus
		case 277239:
			return 21586; // Aspida
		case 277240:
			return 21587; // Atanatos
		case 277241:
			return 21588; // Disillon
		default:
			return 0;
	}
}

// Java AhserionMountableTankAI.java:75-81
bool AhserionMountableTankAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::REWARD_AP_XP_DP_LOOT:
			return false;
		default:
			return ActionItemNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
