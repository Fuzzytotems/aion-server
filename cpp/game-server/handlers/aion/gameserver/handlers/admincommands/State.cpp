#include "aion/gameserver/handlers/admincommands/State.h"

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/state/CreatureStateInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/EnumValueOf.h"
#include "aion/gameserver/utils/JavaColor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(State);

State::State()
	: AdminCommand("state", "Views and adjusts your target's creature states.",
		  " - Shows your target's creature states.\n"
		  "<state> - Sets given creature state(s) by name or ID, replacing existing states.\n"
		  "add <state> - Sets given creature state(s) by name or ID.\n"
		  "remove <state> - Removes given creature state(s) by name or ID. Use -1 to remove all states.\n"
		  "list - Shows possible state names and ID. Add ID values together to add or remove multiple states at once.\n") {
}

// Java State.java:33-91
void State::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	using model::gameobjects::state::getId;
	runtime::Ptr<VisibleObject> target = admin.getTarget();
	if (target == nullptr) {
		sendInfo(admin);
		return;
	}
	runtime::Ptr<Creature> creature = runtime::as<Creature>(target);
	if (creature == nullptr) {
		PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
		return;
	}
	if (params.empty()) {
		sendInfo(admin, name(*creature) + "'s state: " + getStateDescription(creature->getState()) + "\nSee " +
							ChatUtil::color(getAliasWithPrefix() + " help", utils::JavaColor::WHITE) + " for more options.");
	} else if (equalsIgnoreCase("list", params[0])) {
		std::string states;
		const auto& names = xml::EnumTraits<CreatureState>::names;
		for (size_t ordinal = 0; ordinal < names.size(); ordinal++)
			states += (ordinal == 0 ? "" : "\n\t") + std::string(names[ordinal]) + " (" + std::to_string(getId(static_cast<CreatureState>(ordinal))) + ')';
		sendInfo(admin, "Known states:\n\t" + states);
	} else {
		size_t stateIndex = equalsIgnoreCase("add", params[0]) || equalsIgnoreCase("remove", params[0]) ? 1 : 0;
		if (params.size() <= stateIndex) {
			sendInfo(admin, "Please provide a state name or ID.");
			return;
		}
		int32_t stateId;
		try {
			stateId = getId(utils::enumValueOf<CreatureState>(commons::utils::StringUtils::toUpperCase(params[stateIndex])));
		} catch (const commons::utils::IllegalArgumentException&) {
			stateId = commons::utils::parseInt(params[stateIndex]);
			// Correction of the Java code (owner's decision 2026-10-05, both branches; docs/deviations/C1.md): the syntax promises
			// "remove <state> ... Use -1 to remove all states", which Java's range check refused
			if (stateId == -1 && commons::utils::StringUtils::equalsIgnoreCase("remove", params[0]))
				stateId = 0xFFFF;
			if (stateId < 0 || stateId > 0xFFFF) {
				sendInfo(admin, "Out of range state ID.");
				return;
			}
		}
		int32_t newState;
		if (stateIndex == 0)
			newState = stateId & 0xFFFF;
		else if (equalsIgnoreCase("add", params[0]))
			newState = (creature->getState() | stateId) & 0xFFFF;
		else
			newState = (creature->getState() & ~stateId) & 0xFFFF;
		creature->setState(newState);
		if (runtime::Ptr<Player> player = runtime::as<Player>(target)) {
			player->getController().onChangedPlayerAttributes();
		} else {
			creature->clearKnownlist();
			creature->updateKnownlist();
		}
		VisibleObject& targetObject = *target;
		ThreadPoolManager::getInstance().schedule({&admin, &targetObject}, [&admin, &targetObject] {
			admin.setTarget(runtime::Ptr<VisibleObject>(&targetObject));
		}, 200);
		sendInfo(admin, name(*creature) + "'s state changed to " + getStateDescription(creature->getState()));
	}
}

// Java State.java:93-103
std::string State::getStateDescription(int32_t state) {
	std::string sb;
	for (int32_t i = 1; i <= (state & 0xFFFF); i *= 2) {
		if ((state & i) == i) {
			if (!sb.empty())
				sb += " + ";
			sb += findStateName(i, "UNK") + " (" + std::to_string(i) + ')';
		}
	}
	return std::to_string(state) + (sb.empty() ? "" : " = " + sb);
}

// Java State.java:105-107
std::string State::findStateName(int32_t creatureStateId, std::string_view defaultName) {
	using model::gameobjects::state::getId;
	const auto& names = xml::EnumTraits<CreatureState>::names;
	for (size_t ordinal = 0; ordinal < names.size(); ordinal++) {
		if (getId(static_cast<CreatureState>(ordinal)) == creatureStateId)
			return std::string(names[ordinal]);
	}
	return std::string(defaultName);
}

} // namespace aion::gameserver::handlers::admincommands
