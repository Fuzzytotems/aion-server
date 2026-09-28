#include "aion/gameserver/services/CommandsAccessService.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "aion/gameserver/dao/CommandsAccessDAO.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"

namespace aion::gameserver::services {

void CommandsAccessService::loadAccesses() {
	// Java: commandAccesses = CommandsAccessDAO.loadAccesses() (a HashMap<Integer, Set<String>> the DAO fills)
	runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<runtime::RcHashSet<std::string>>>> accesses =
		runtime::RcHashMap<int32_t, runtime::Ref<runtime::RcHashSet<std::string>>>::create(AION_LOCK_CLASS(CommandsAccessService::commandAccesses));
	for (const auto& [playerId, commands] : dao::CommandsAccessDAO::loadAccesses()) {
		runtime::Ref<runtime::RcHashSet<std::string>> set = runtime::RcHashSet<std::string>::create(AION_LOCK_CLASS(CommandsAccessService::commandAccesses#set));
		for (const std::string& command : commands)
			set->add(command);
		accesses->put(playerId, set);
	}
	commandAccesses.set(accesses);
}

void CommandsAccessService::giveTemporaryAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command) {
	giveAccess(admin, playerId, command, true);
}

void CommandsAccessService::giveAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command) {
	giveAccess(admin, playerId, command, false);
}

void CommandsAccessService::giveAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command, bool isTemporary) {
	if (hasAccess(playerId, command)) {
		utils::PacketSendUtility::sendMessage(admin, "This player already has access on command " + std::string(command));
		return;
	}
	if (!utils::chathandlers::ChatProcessor::getInstance().isCommandExists(command)) {
		utils::PacketSendUtility::sendMessage(admin, "There is no such admin command as \"" + std::string(command) + "\"");
		return;
	}
	// Java: commandAccesses.compute(playerId, ...) on the map loadAccesses stored (Collections.emptyMap() before: UnsupportedOperationException)
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<runtime::RcHashSet<std::string>>>> accesses = commandAccesses.get();
	if (!accesses)
		throw runtime::UnsupportedOperationException("compute on Collections.emptyMap()");
	accesses->compute(playerId, [&command](runtime::Ptr<runtime::RcHashSet<std::string>> commands) {
		runtime::Ref<runtime::RcHashSet<std::string>> result(commands);
		if (!result)
			result = runtime::RcHashSet<std::string>::create(AION_LOCK_CLASS(CommandsAccessService::commandAccesses#set));
		result->add(std::string(command));
		return result;
	});
	if (!isTemporary)
		dao::CommandsAccessDAO::addAccess(playerId, command);
	utils::PacketSendUtility::sendMessage(admin, "Command access was granted successfuly.");
}

void CommandsAccessService::removeAccess(model::gameobjects::player::Player& admin, int32_t playerId, std::string_view command) {
	if (!hasAccess(playerId, command)) {
		utils::PacketSendUtility::sendMessage(admin, "This player has no access on command " + std::string(command));
		return;
	}
	runtime::Ptr<runtime::RcHashSet<std::string>> commands = commandAccesses.get()->get(playerId);
	commands->remove(std::string(command));
	dao::CommandsAccessDAO::removeAccess(playerId, command);
	utils::PacketSendUtility::sendMessage(admin, "Command access was removed successfully");
}

bool CommandsAccessService::removeAllAccesses(int32_t playerId) {
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<runtime::RcHashSet<std::string>>>> accesses = commandAccesses.get();
	runtime::Ptr<runtime::RcHashSet<std::string>> commands = accesses ? accesses->get(playerId) : nullptr;
	if (commands) {
		commands->clear();
		dao::CommandsAccessDAO::removeAllAccesses(playerId);
		return true;
	}
	return false;
}

bool CommandsAccessService::hasAccess(int32_t playerId, std::string_view command) {
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<runtime::RcHashSet<std::string>>>> accesses = commandAccesses.get();
	runtime::Ptr<runtime::RcHashSet<std::string>> commands = accesses ? accesses->get(playerId) : nullptr;
	return commands && commands->contains(std::string(command));
}

} // namespace aion::gameserver::services
