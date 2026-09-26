#include "aion/gameserver/skillengine/properties/TargetRelationProperty.h"

#include <optional>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/templates/npc/AbyssNpcType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/properties/TargetRelationAttribute.h"

namespace aion::gameserver::skillengine::properties {

using dataholders::DataManager;
using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::gameobjects::siege::SiegeNpc;
using gameserver::model::templates::npc::AbyssNpcType;
using runtime::Ptr;
using runtime::Ref;

bool TargetRelationProperty::set(const Properties* properties, Properties::ValidationResult& result, Creature& effector,
	const model::SkillTemplate* skillTemplate) {
	// Java: `TargetRelationAttribute value = properties.getTargetRelation()`; Properties.validateEffectedList runs this step only when it is set
	TargetRelationAttribute value = *properties->getTargetRelation();
	switch (value) {
		case TargetRelationAttribute::ALL:
			break;
		case TargetRelationAttribute::ENEMY:
			if (!DataManager::MATERIAL_DATA->isMaterialSkill(skillTemplate->getSkillId()))
				result.getTargets().removeIf([&effector](const Ptr<Creature>& target) { return !effector.isEnemy(*target); });
			break;
		case TargetRelationAttribute::FRIEND:
			if (!DataManager::MATERIAL_DATA->isMaterialSkill(skillTemplate->getSkillId()))
				result.getTargets().removeIf(
					[&effector](const Ptr<Creature>& target) { return effector.isEnemy(*target) || !isBuffAllowed(Ptr<Creature>(effector), target); });

			if (result.getTargets().isEmpty()) {
				result.setFirstTarget(effector);
				result.getTargets().add(Ref<Creature>(effector));
			} else {
				result.setFirstTarget(result.getTargets().get(0)); // Java: getFirst()
			}
			break;
		case TargetRelationAttribute::MYPARTY: {
			for (auto iter = result.getTargets().iterator(); iter.hasNext();) {
				Ptr<Creature> target = iter.next();
				if (Ptr<Player> sourcePlayer = runtime::as<Player>(effector.getMaster()); sourcePlayer && isBuffAllowed(Ptr<Creature>(effector), target)) {
					if (target->getMaster()->equals(*sourcePlayer))
						continue;
					if (Ptr<Player> targetPlayer = runtime::as<Player>(target->getMaster())) {
						int32_t teamId = sourcePlayer->getCurrentTeamId();
						if (teamId > 0 && teamId == targetPlayer->getCurrentTeamId() && !sourcePlayer->isEnemy(*targetPlayer))
							continue;
					}
				}
				iter.remove();
			}

			if (!result.getTargets().isEmpty()) {
				result.setFirstTarget(result.getTargets().get(0)); // Java: getFirst()
			}
			break;
		}
		default: // Java: NONE has no arm
			break;
	}

	return true;
}

bool TargetRelationProperty::isBuffAllowed(Ptr<Creature> source, Ptr<Creature> target) {
	if (!source || !target) {
		return false;
	}

	if (Ptr<SiegeNpc> siegeNpc = runtime::as<SiegeNpc>(target)) {
		switch (siegeNpc->getObjectTemplate()->getAbyssNpcType()) {
			case AbyssNpcType::ARTIFACT:
			case AbyssNpcType::ARTIFACT_EFFECT_CORE:
			case AbyssNpcType::DOOR:
			case AbyssNpcType::DOORREPAIR:
				return false;
			default:
				break;
		}
	}

	return isSameAreaType(*source, *target);
}

bool TargetRelationProperty::isSameAreaType(Creature& source, Creature& target) {
	return source.isInsidePvPZone() == target.isInsidePvPZone();
}

} // namespace aion::gameserver::skillengine::properties
