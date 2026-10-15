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
	auto param = [&params](size_t index) -> const std::string& { // parity: Java's implicit ArrayIndexOutOfBoundsException of params[i], explicit (the comment above)
		if (index >= params.size()) // parity: (the same)
			throw runtime::ArrayIndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(params.size())); // parity: (the same)
		return params[index]; // parity: (the same)
	};
	try {
		std::optional<std::string> targetMode = commons::utils::StringUtils::toLowerCase(params[0]);
		size_t i = 0;
		if (*targetMode == "me" || *targetMode == "self" || *targetMode == "target") // parity= switch (targetMode) { case "me": case "self": case "target":
			i++;
		else
			targetMode = std::nullopt;
		// Correction of the Java code (owner's decision 2026-10-05, both branches; docs/deviations/C1.md): a target mode without a skill ID
		// shows the syntax (Java's params[i++] threw ArrayIndexOutOfBoundsException)
		if (params.size() <= i) { // parity: the owner's correction of 2026-10-05 above (a target mode without a skill ID)
			sendInfo(admin); // parity: (continued)
			return;
		}
		const SkillTemplate* template_ = DataManager::SKILL_DATA->getSkillTemplate(commons::utils::parseInt(param(i++))); // parity= SkillTemplate template = DataManager.SKILL_DATA.getSkillTemplate(Integer.parseInt(params[i++]));
		if (template_ != nullptr) {
			int32_t skillLevel = params.size() > i && params[i] != "f" ? commons::utils::parseInt(params[i++]) : template_->getLvl(); // parity= int skillLevel = params.length > i && !params[i].equals("f") ? Integer.parseInt(params[i++]) : template.getLvl();
			bool forceUse = params.size() > i && params[i] == "f";
			if (useSkill(admin, *template_, skillLevel, targetMode, forceUse))
				sendInfo(admin, "Used skill: " + template_->getL10n());
			else
				sendInfo(admin, "Could not use skill (" + std::string(forceUse ? "missing preconditions" : "add parameter 'f' to force use") + ")."); // parity= sendInfo(admin, "Could not use skill (" + (forceUse ? "missing preconditions" : "add parameter 'f' to force use") + ").");
		} else {
			sendInfo(admin, "Invalid skill ID.");
		}
	} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException _) {
		sendInfo(admin, "Invalid skill ID or level.");
	}
}

// Java UseSkill.java:64-82
bool UseSkill::useSkill(Player& player, const SkillTemplate& template_, int32_t skillLevel, std::optional<std::string_view> targetMode, bool forceUse) {
	runtime::Ptr<Creature> effector;
	runtime::Ptr<VisibleObject> target;
	if (targetMode) { // parity= if (targetMode != null) {
		runtime::Ptr<Creature> creatureTarget = runtime::as<Creature>(player.getTarget());
		if (creatureTarget == nullptr) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
			return false;
		}
		effector = creatureTarget;
		target = getTarget(player, *targetMode);
	} else {
		effector = runtime::Ptr<Creature>(&player); // parity= effector = player;
		target = player.getTarget();
	}
	runtime::Ref<Skill> skill = SkillEngine::getInstance().getSkill(*effector, template_.getSkillId(), skillLevel, target);
	if (skill) // parity= if (skill != null)
		return forceUse ? skill->useWithoutPropSkill() : skill->useNoAnimationSkill();
	return false;
}

// Java UseSkill.java:84-91
runtime::Ptr<VisibleObject> UseSkill::getTarget(Player& player, std::string_view targetMode) {
	if (targetMode == "me") // parity= return switch (targetMode) { case "me" -> player;
		return runtime::Ptr<VisibleObject>(&player); // parity: (continued)
	if (targetMode == "self") // parity= case "self" -> player.getTarget();
		return player.getTarget(); // parity: (continued)
	if (targetMode == "target") // parity= case "target" -> player.getTarget() == null ? null : player.getTarget().getTarget();
		return player.getTarget() == nullptr ? nullptr : player.getTarget()->getTarget(); // parity: (continued)
	return nullptr; // parity= default -> null;
}

} // namespace aion::gameserver::handlers::admincommands
