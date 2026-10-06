#include "aion/gameserver/handlers/admincommands/Ai.h"

#include <memory>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/ai/AIEngine.h"
#include "aion/gameserver/ai/AISubState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/event/AIEventLog.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/utils/EnumValueOf.h"
#include "aion/gameserver/utils/SimpleClassName.h"

namespace aion::gameserver::handlers::admincommands {

AION_ADMIN_COMMAND(Ai);

Ai::Ai()
	: AdminCommand("ai", "Modifies and shows AI details.",
		  "info - Show AI info for your target.\n"
		  "set <aiName> - Changes the AI of your target.\n"
		  "state <stateName> [substateName] - Changes the AI state.\n"
		  "event <eventName> - Fires the AI event for the given name.\n"
		  "event2 <eventName> <creatureObjId> - Fires the creature AI event for the given name and creature.\n"
		  "events - Shows last AI events for your target.\n"
		  "log - Toggles AI logging for your target on and off.\n"
		  "<createlog|eventlog|movelog> - Toggles logging on and off.\n"
		  "marker [text] - Prints a marker with the optional text in log.\n") {
}

// Java Ai.java:37-128
void Ai::execute(Player& admin, std::span<const std::string> params) {
	using commons::utils::StringUtils::equalsIgnoreCase;
	using commons::utils::StringUtils::toUpperCase;
	using ai::AISubState;
	auto boolText = [](bool value) { return std::string(value ? "true" : "false"); };
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	if (equalsIgnoreCase(params[0], "createlog")) {
		AIConfig::ONCREATE_DEBUG.store(!AIConfig::ONCREATE_DEBUG.load()); // Java: an unsynchronized read-modify-write of a static field
		sendInfo(admin, "New createlog value: " + boolText(AIConfig::ONCREATE_DEBUG.load()));
	} else if (equalsIgnoreCase(params[0], "eventlog")) {
		AIConfig::EVENT_DEBUG.store(!AIConfig::EVENT_DEBUG.load());
		sendInfo(admin, "New eventlog value: " + boolText(AIConfig::EVENT_DEBUG.load()));
	} else if (equalsIgnoreCase(params[0], "movelog")) {
		AIConfig::MOVE_DEBUG.store(!AIConfig::MOVE_DEBUG.load());
		sendInfo(admin, "New movelog value: " + boolText(AIConfig::MOVE_DEBUG.load()));
	} else if (equalsIgnoreCase(params[0], "marker")) {
		// Java: LoggerFactory.getLogger(AILogger.class)
		const auto aiLog = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.ai.AILogger");
		if (params.size() > 1)
			aiLog.info("[AI] marker: " + join(params, 1));
		else
			aiLog.info("[AI] marker");
	} else {
		runtime::Ptr<Creature> npc = runtime::as<Creature>(admin.getTarget());
		if (npc == nullptr || runtime::as<Player>(npc) != nullptr) {
			PacketSendUtility::sendPacket(admin, SM_SYSTEM_MESSAGE::STR_INVALID_TARGET());
			return;
		}

		if (equalsIgnoreCase(params[0], "info")) {
			sendInfo(admin, "[AI info]\n\tName: " + npc->getAi().getName() + "\n\tState: " + std::string(xml::enumName(npc->getAi().getState())) +
								"\n\tSubstate: " + std::string(xml::enumName(npc->getAi().getSubState())));
		} else if (equalsIgnoreCase(params[0], "log")) {
			bool oldValue = npc->getAi().isLogging();
			npc->getAi().setLogging(!oldValue);
			sendInfo(admin, "New log value: " + boolText(!oldValue));
		} else if (equalsIgnoreCase(params[0], "events")) {
			runtime::Ptr<AIEventLog> eventLog = npc->getAi().getEventLog();
			if (eventLog == nullptr || eventLog->isEmpty()) {
				sendInfo(admin, "No events logged" + std::string(AIConfig::EVENT_DEBUG.load() ? "" : " (enable event logging via eventlog parameter)"));
			} else {
				for (AIEventType eventType : *eventLog) {
					sendInfo(admin, "EVENT: " + std::string(xml::enumName(eventType)));
				}
			}
		} else if (params.size() > 1) {
			const std::string& param1 = params[1];
			if (equalsIgnoreCase(params[0], "set")) {
				const std::string& aiName = param1;
				std::unique_ptr<ai::AbstractAI> newAi = AIEngine::getInstance().newAI(aiName, *npc);
				// Java: the private final field "ai" is replaced by reflection between a despawn and a spawn; its reflection errors cannot
				// happen in C++ (Creature::replaceAi, docs/deviations/C2.md)
				std::string newAiName = utils::simpleClassName(typeid(*newAi));
				World::getInstance().despawn(*npc, ObjectDeleteAnimation::NONE);
				npc->replaceAi(std::move(newAi));
				World::getInstance().spawn(npc); // properly init AI states
				sendInfo(admin, "Npc now has AI " + newAiName);
			} else if (equalsIgnoreCase(params[0], "event")) {
				AIEventType eventType = utils::enumValueOf<AIEventType>(toUpperCase(param1));
				npc->getAi().onGeneralEvent(eventType);
			} else if (equalsIgnoreCase(params[0], "event2")) {
				runtime::Ptr<Creature> creature;
				if (params.size() >= 3) {
					// Correction of the Java code (owner's decision 2026-10-05, both branches; docs/deviations/C2.md): an object that is no
					// creature is answered like an unknown ID (Java's (Creature) cast threw ClassCastException)
					creature = runtime::as<Creature>(World::getInstance().findVisibleObject(commons::utils::parseInt(params[2])));
				}
				if (creature == nullptr)
					sendInfo(admin, "Please provide a valid creature object ID");
				else {
					AIEventType eventType = utils::enumValueOf<AIEventType>(toUpperCase(param1));
					npc->getAi().onCreatureEvent(eventType, *creature);
				}
			} else if (equalsIgnoreCase(params[0], "state")) {
				AIState state = utils::enumValueOf<AIState>(toUpperCase(param1));
				npc->getAi().setStateIfNot(state);
				if (params.size() > 2) {
					AISubState substate = utils::enumValueOf<AISubState>(params[2]); // Java: no toUpperCase here
					npc->getAi().setSubStateIfNot(substate);
				}
			}
		} else {
			sendInfo(admin);
		}
	}
}

} // namespace aion::gameserver::handlers::admincommands
