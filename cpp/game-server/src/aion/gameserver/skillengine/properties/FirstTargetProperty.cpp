#include "aion/gameserver/skillengine/properties/FirstTargetProperty.h"

#include <optional>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/DispelCategoryType.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/properties/FirstTargetAttribute.h"
#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/skillengine/properties/TargetRangeAttribute.h"
#include "aion/gameserver/skillengine/properties/TargetRelationAttribute.h"
#include "aion/gameserver/skillengine/properties/TargetRelationProperty.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::skillengine::properties {

using gameserver::model::gameobjects::AionObject;
using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::Summon;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::team::TemporaryPlayerTeam;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using runtime::Ptr;
using utils::PacketSendUtility;
using utils::PositionUtil;

namespace {

/** Java `switch (properties.getTargetRelation())` over a null enum: the implicit ordinal() call throws NullPointerException */
TargetRelationAttribute switchOn(std::optional<TargetRelationAttribute> relation) {
	if (!relation.has_value())
		throw runtime::NullPointerException(
			"Cannot invoke \"TargetRelationAttribute.ordinal()\" because the return value of \"Properties.getTargetRelation()\" is null");
	return *relation;
}

/** Java `a.equals(b)` of AionObject with a nullable argument: false for null */
bool equalsNullable(AionObject& a, Ptr<Creature> b) {
	return b && a.equals(*b);
}

} // namespace

bool FirstTargetProperty::set(model::Skill& skill, const Properties* properties) {
	Ptr<Creature> effector = skill.getEffector();
	switch (properties->getFirstTarget()) {
		case FirstTargetAttribute::ME:
			skill.setFirstTargetRangeCheck(false);
			skill.setFirstTarget(effector);
			break;
		case FirstTargetAttribute::TARGETORME: {
			if (equalsNullable(*effector, skill.getFirstTarget()))
				break;
			bool changeTargetToMe = false;
			if (!skill.getFirstTarget()) {
				changeTargetToMe = true;
			} else {
				switch (switchOn(properties->getTargetRelation())) {
					case TargetRelationAttribute::ENEMY:
						if (!skill.getFirstTarget()->isEnemy(*effector))
							changeTargetToMe = true;
						break;
					case TargetRelationAttribute::FRIEND:
						if (skill.getFirstTarget()->isEnemy(*effector))
							changeTargetToMe = true;
						break;
					case TargetRelationAttribute::MYPARTY:
						if (!isTargetTeamMember(skill, false)) {
							if (skill.getFirstTarget()->isEnemy(*effector)) {
								changeTargetToMe = true;
							} else {
								// Java: (Player) effector - a ClassCastException for any other effector
								PacketSendUtility::sendPacket(*runtime::cast<Player>(effector), SM_SYSTEM_MESSAGE::STR_SKILL_INVALID_TARGET_PARTY_ONLY());
								return false;
							}
						}
						break;
					default:
						break;
				}
				if (!changeTargetToMe && !isTargetAllowed(skill, skill.getFirstTarget()))
					changeTargetToMe = true;
			}
			if (changeTargetToMe) {
				if (skill.getFirstTarget()) {
					if (Ptr<Player> playerEffector = runtime::as<Player>(effector))
						PacketSendUtility::sendPacket(*playerEffector, SM_SYSTEM_MESSAGE::STR_SKILL_AUTO_CHANGE_TARGET_TO_MY());
				}
				skill.setFirstTarget(effector);
			}
			break;
		}
		case FirstTargetAttribute::TARGET: {
			// Exception for effect skills which are not used directly
			if (skill.getSkillId() > 8000 && skill.getSkillId() < 9000)
				break;
			// Exception for NPC skills which applied on players
			if (skill.getSkillTemplate()->getDispelCategory() == model::DispelCategoryType::NPC_BUFF
				|| skill.getSkillTemplate()->getDispelCategory() == model::DispelCategoryType::NPC_DEBUFF_PHYSICAL)
				break;

			std::optional<TargetRelationAttribute> relation = skill.getSkillTemplate()->getProperties()->getTargetRelation();
			if (!skill.getFirstTarget() || skill.getFirstTarget()->equals(*effector)) {
				if (Ptr<Player> playerEffector = runtime::as<Player>(effector)) {
					if (skill.getSkillTemplate()->getProperties()->getTargetType() == TargetRangeAttribute::AREA)
						return static_cast<bool>(skill.getFirstTarget());

					std::optional<TargetRangeAttribute> type = skill.getSkillTemplate()->getProperties()->getTargetType();
					if ((relation != TargetRelationAttribute::ALL && relation != TargetRelationAttribute::MYPARTY && relation != TargetRelationAttribute::FRIEND)
						|| type == TargetRangeAttribute::PARTY) {
						PacketSendUtility::sendPacket(*playerEffector, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID());
						return false;
					}
				}
			}

			if (relation == TargetRelationAttribute::FRIEND) {
				if (!skill.getFirstTarget() || effector->isEnemy(*skill.getFirstTarget())) {
					if (Ptr<Player> playerEffector = runtime::as<Player>(effector))
						PacketSendUtility::sendPacket(*playerEffector, SM_SYSTEM_MESSAGE::STR_SKILL_INVALID_TARGET_NOTENEMY_ONLY());
					return false;
				}
			} else if (relation == TargetRelationAttribute::MYPARTY) {
				if (!isTargetTeamMember(skill, false)) {
					if (Ptr<Player> playerEffector = runtime::as<Player>(effector))
						PacketSendUtility::sendPacket(*playerEffector, SM_SYSTEM_MESSAGE::STR_SKILL_INVALID_TARGET_PARTY_ONLY());
					return false;
				}
			} else if (relation != TargetRelationAttribute::ENEMY && !isTargetAllowed(skill, skill.getFirstTarget())) {
				if (Ptr<Player> playerEffector = runtime::as<Player>(effector))
					PacketSendUtility::sendPacket(*playerEffector, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID());
				return false;
			}
			break;
		}
		case FirstTargetAttribute::MYPET:
			if (Ptr<Player> playerEffector = runtime::as<Player>(effector)) {
				Ptr<Summon> summon = playerEffector->getSummon();
				if (!summon || !isTargetAllowed(skill, summon)) {
					PacketSendUtility::sendPacket(*playerEffector, SM_SYSTEM_MESSAGE::STR_SKILL_INVALID_TARGET_PET_ONLY());
					return false;
				}
				skill.setFirstTarget(summon);
			} else {
				return false;
			}
			break;
		case FirstTargetAttribute::MYMASTER:
			if (Ptr<Summon> summon = runtime::as<Summon>(effector)) {
				if (summon->getMaster())
					skill.setFirstTarget(summon->getMaster());
				else
					return false;
			} else {
				return false;
			}
			break;
		case FirstTargetAttribute::PASSIVE:
			skill.setFirstTarget(effector);
			break;
		case FirstTargetAttribute::TARGET_MYPARTY_NONVISIBLE: // Summon Group Member
			if (!isTargetTeamMember(skill, true)) {
				if (Ptr<Player> playerEffector = runtime::as<Player>(effector))
					PacketSendUtility::sendPacket(*playerEffector, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID());
				return false;
			}
			skill.setFirstTargetRangeCheck(false);
			break;
		case FirstTargetAttribute::POINT:
			skill.setFirstTarget(effector);
			return true;
		default: // Java: a switch without a default arm (every constant is listed above)
			break;
	}

	if (Ptr<Creature> firstTarget = skill.getFirstTarget()) {
		// update heading for npcs (players may look in a different direction)
		if (runtime::as<Npc>(effector) && !effector->equals(*firstTarget))
			effector->getPosition()->setH(PositionUtil::getHeadingTowards(*effector, *firstTarget));
		skill.getEffectedList().add(runtime::Ref<Creature>(firstTarget));
	}
	return true;
}

bool FirstTargetProperty::isTargetTeamMember(model::Skill& skill, bool onlyGroup) {
	if (runtime::as<Player>(skill.getFirstTarget()) && runtime::as<Player>(skill.getEffector())) {
		Ptr<Player> effector = runtime::cast<Player>(skill.getEffector());
		Ptr<TemporaryPlayerTeam> team = onlyGroup ? effector->getCurrentGroup() : effector->getCurrentTeam();
		if (team) {
			for (Ptr<AionObject> member : team->getMembers()) {
				if (equalsNullable(*member, skill.getFirstTarget()) && !equalsNullable(*member, skill.getEffector()))
					return true;
			}
		}
	}
	return false;
}

bool FirstTargetProperty::isTargetAllowed(model::Skill& skill, Ptr<Creature> target) {
	Ptr<Creature> source = skill.getEffector();
	return TargetRelationProperty::isBuffAllowed(source, target);
}

} // namespace aion::gameserver::skillengine::properties
