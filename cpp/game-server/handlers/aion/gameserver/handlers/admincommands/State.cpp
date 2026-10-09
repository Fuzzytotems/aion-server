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
		std::string states; // parity= sendInfo(admin, "Known states:\n\t" + Arrays.stream(CreatureState.values()).map(c -> c.name() + " (" + c.getId() + ')').collect(Collectors.joining("\n\t")));
		const auto& names = xml::EnumTraits<CreatureState>::names; // parity: (continued)
		for (size_t ordinal = 0; ordinal < names.size(); ordinal++) // parity: (continued)
			states += (ordinal == 0 ? "" : "\n\t") + std::string(names[ordinal]) + " (" + std::to_string(getId(static_cast<CreatureState>(ordinal))) + ')'; // parity: (continued)
		sendInfo(admin, "Known states:\n\t" + states); // parity: (continued)
	} else {
		size_t stateIndex = equalsIgnoreCase("add", params[0]) || equalsIgnoreCase("remove", params[0]) ? 1 : 0;
		if (params.size() <= stateIndex) {
			sendInfo(admin, "Please provide a state name or ID.");
			return;
		}
		int32_t stateId;
		try {
			stateId = getId(utils::enumValueOf<CreatureState>(commons::utils::StringUtils::toUpperCase(params[stateIndex]))); // parity= stateId = CreatureState.valueOf(params[stateIndex].toUpperCase()).getId();
		} catch (const commons::utils::IllegalArgumentException&) { // parity= } catch (IllegalArgumentException e) {
			stateId = commons::utils::parseInt(params[stateIndex]);
			// Correction of the Java code (owner's decision 2026-10-05, both branches; docs/deviations/C1.md): the syntax promises
			// "remove <state> ... Use -1 to remove all states", which Java's range check refused
			if (stateId == -1 && commons::utils::StringUtils::equalsIgnoreCase("remove", params[0])) // parity: the owner's correction of 2026-10-05 above (remove -1 removes every state)
				stateId = 0xFFFF; // parity: (continued)
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
		if (runtime::Ptr<Player> player = runtime::as<Player>(target)) { // parity= if (target instanceof Player player) {
			player->getController().onChangedPlayerAttributes();
		} else {
			creature->clearKnownlist();
			creature->updateKnownlist();
		}
		VisibleObject& targetObject = *target; // parity: the pinned capture of the scheduled lambda (docs/deviations/C1.md)
		ThreadPoolManager::getInstance().schedule({&admin, &targetObject}, [&admin, &targetObject] { // parity= ThreadPoolManager.getInstance().schedule(() -> admin.setTarget(target), 200);
			admin.setTarget(runtime::Ptr<VisibleObject>(&targetObject)); // parity: (continued)
		}, 200); // parity: (continued)
		sendInfo(admin, name(*creature) + "'s state changed to " + getStateDescription(creature->getState()));
	}
}

// Java State.java:93-103
std::string State::getStateDescription(int32_t state) {
	std::string sb; // parity= StringBuilder sb = new StringBuilder();
	for (int32_t i = 1; i <= (state & 0xFFFF); i *= 2) {
		if ((state & i) == i) {
			if (!sb.empty())
				sb += " + "; // parity= sb.append(" + ");
			sb += findStateName(i, "UNK") + " (" + std::to_string(i) + ')'; // parity= sb.append(findStateName(i, "UNK")).append(" (").append(i).append(')');
		}
	}
	return std::to_string(state) + (sb.empty() ? "" : " = " + sb); // parity= return state + (sb.isEmpty() ? "" : " = " + sb.toString());
}

// Java State.java:105-107
std::string State::findStateName(int32_t creatureStateId, std::string_view defaultName) {
	using model::gameobjects::state::getId;
	const auto& names = xml::EnumTraits<CreatureState>::names; // parity= return Arrays.stream(CreatureState.values()).filter(s -> s.getId() == creatureStateId).findFirst().map(Object::toString).orElse(defaultName);
	for (size_t ordinal = 0; ordinal < names.size(); ordinal++) { // parity: (continued)
		if (getId(static_cast<CreatureState>(ordinal)) == creatureStateId) // parity: (continued)
			return std::string(names[ordinal]); // parity: (continued)
	} // parity: (continued)
	return std::string(defaultName); // parity: (continued)
}

} // namespace aion::gameserver::handlers::admincommands
