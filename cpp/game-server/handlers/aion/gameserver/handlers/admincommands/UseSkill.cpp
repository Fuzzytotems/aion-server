#include "aion/gameserver/handlers/admincommands/UseSkill.h"

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(UseSkill);

UseSkill::UseSkill()
	: AdminCommand("useskill", "Use (or let a target use) any skill, even those not in skill list.",
		  "<id> [lvl] [f] - Uses the skill with the specified skill level on your target (f = force use).\n"
		  "<me|self|target> <id> [lvl] [f] - Lets your target use the skill on you, itself or its target (f = force use).\n") {
}

// Java UseSkill.java:31-62
void UseSkill::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	// Java: params[i] past the end throws ArrayIndexOutOfBoundsException (not caught here: ChatCommand.run logs it)
	auto param = [&params](size_t index) -> const std::string& {
		if (index >= params.size())
			throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(params.size()));
		return params[index];
	};
	try {
		std::optional<std::string> targetMode = commons::utils::StringUtils::toLowerCase(params[0]);
		size_t i = 0;
		if (*targetMode == "me" || *targetMode == "self" || *targetMode == "target")
			i++;
		else
			targetMode = std::nullopt;
		const SkillTemplate* template_ = DataManager::SKILL_DATA->getSkillTemplate(commons::utils::parseInt(param(i++)));
		if (template_ != nullptr) {
			int32_t skillLevel = params.size() > i && params[i] != "f" ? commons::utils::parseInt(params[i++]) : template_->getLvl();
			bool forceUse = params.size() > i && params[i] == "f";
			if (useSkill(admin, *template_, skillLevel, targetMode, forceUse))
				sendInfo(admin, "Used skill: " + template_->getL10n());
			else
				sendInfo(admin, "Could not use skill (" + std::string(forceUse ? "missing preconditions" : "add parameter 'f' to force use") + ").");
		} else {
			sendInfo(admin, "Invalid skill ID.");
		}
	} catch (const commons::utils::NumberFormatException&) {
		sendInfo(admin, "Invalid skill ID or level.");
	}
}

// Java UseSkill.java:64-82
bool UseSkill::useSkill(Player& player, const SkillTemplate& template_, int32_t skillLevel, std::optional<std::string_view> targetMode, bool forceUse) {
	runtime::Ptr<Creature> effector;
	runtime::Ptr<VisibleObject> target;
	if (targetMode) {
		runtime::Ptr<Creature> creatureTarget = runtime::as<Creature>(player.getTarget());
		if (creatureTarget == nullptr) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
			return false;
		}
		effector = creatureTarget;
		target = getTarget(player, *targetMode);
	} else {
		effector = runtime::Ptr<Creature>(&player);
		target = player.getTarget();
	}
	runtime::Ref<Skill> skill = SkillEngine::getInstance().getSkill(*effector, template_.getSkillId(), skillLevel, target);
	if (skill)
		return forceUse ? skill->useWithoutPropSkill() : skill->useNoAnimationSkill();
	return false;
}

// Java UseSkill.java:84-91
runtime::Ptr<VisibleObject> UseSkill::getTarget(Player& player, std::string_view targetMode) {
	if (targetMode == "me")
		return runtime::Ptr<VisibleObject>(&player);
	if (targetMode == "self")
		return player.getTarget();
	if (targetMode == "target")
		return player.getTarget() == nullptr ? nullptr : player.getTarget()->getTarget();
	return nullptr;
}

} // namespace aion::gameserver::handlers::admincommands
