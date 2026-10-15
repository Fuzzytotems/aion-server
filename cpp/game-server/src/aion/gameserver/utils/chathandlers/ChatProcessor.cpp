#include "aion/gameserver/utils/chathandlers/ChatProcessor.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <map>
#include <memory>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/Config.h"
#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/utils/chathandlers/AdminCommand.h"
#include "aion/gameserver/utils/chathandlers/ChatCommand.h"
#include "aion/gameserver/utils/chathandlers/ConsoleCommand.h"
#include "aion/gameserver/utils/chathandlers/PlayerCommand.h"

namespace aion::gameserver::utils::chathandlers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.utils.chathandlers.ChatProcessor");

namespace {

/** the data/handlers directory of a command kind (Java: the directory ScriptManager compiles the command class from) */
std::string_view directoryOf(handlers::CommandKind kind) {
	switch (kind) {
		case handlers::CommandKind::ADMIN:
			return "admincommands";
		case handlers::CommandKind::PLAYER:
			return "playercommands";
		case handlers::CommandKind::CONSOLE:
			return "consolecommands";
	}
	return "";
}

/** Java: ScriptManager.load(CommandsConfig.HANDLER_DIRECTORIES) compiles the commands of exactly these directories */
bool isConfiguredDirectory(handlers::CommandKind kind) {
	auto directories = configs::administration::CommandsConfig::HANDLER_DIRECTORIES.get();
	return std::ranges::any_of(*directories, [kind](const std::filesystem::path& directory) {
		std::filesystem::path normalized = directory.lexically_normal();
		if (!normalized.has_filename())
			normalized = normalized.parent_path();
		return normalized.filename().generic_string() == directoryOf(kind);
	});
}

/** Java String.trim() */
std::string_view javaTrim(std::string_view text) {
	while (!text.empty() && static_cast<unsigned char>(text.front()) <= ' ')
		text.remove_prefix(1);
	while (!text.empty() && static_cast<unsigned char>(text.back()) <= ' ')
		text.remove_suffix(1);
	return text;
}

/** Java String.isEmpty() of the UTF-16 string */
bool isEmpty(std::string_view text) {
	return text.empty();
}

} // namespace

ChatProcessor::ChatProcessor() = default;

ChatProcessor::~ChatProcessor() = default;

void ChatProcessor::init() {
	// Java: new ScriptManager with a ChatCommandsLoader over CommandsConfig.HANDLER_DIRECTORIES (class comment)
	for (const handlers::CommandEntry& entry : handlers::commandEntries()) {
		if (!isConfiguredDirectory(entry.kind))
			continue;
		std::unique_ptr<ChatCommand> command = entry.create();
		ChatCommand& registered = *command.release(); // Immortal: never freed (RT-11)
		registerCommand(registered);
	}
	log.info("Loaded " + std::to_string(commandHandlers.size()) + " commands.");
}

void ChatProcessor::reload() {
	std::map<std::string, ChatCommand*> oldCommands;
	for (const auto& entry : commandHandlers.snapshot())
		oldCommands.emplace(entry.key, entry.value);
	try {
		configs::Config::load({&configs::administration::CommandsConfig::bind});
		commandHandlers.clear();
		init();
	} catch (...) {
		commandHandlers.clear();
		for (const auto& [alias, command] : oldCommands)
			commandHandlers.put(alias, command);
		throw;
	}
}

void ChatProcessor::registerCommand(ChatCommand& cmd) {
	std::string cmdName = cmd.getAliasWithPrefix();
	if (cmd.getLevel() < 0)
		throw runtime::IllegalArgumentException("Failed to register " + utils::simpleClassName(typeid(cmd)) + ": Invalid access level for " + cmdName +
			".");
	if (commandHandlers.putIfAbsent(commons::utils::StringUtils::toLowerCase(cmdName), &cmd) != nullptr)
		throw runtime::IllegalArgumentException("Failed to register " + utils::simpleClassName(typeid(cmd)) + ": " + cmdName + " is already registered.");
}

bool ChatProcessor::handleChatCommand(model::gameobjects::player::Player& player, std::string_view text) {
	if (isEmpty(text))
		return false;
	if (!text.starts_with(AdminCommand::PREFIX) && !text.starts_with(PlayerCommand::PREFIX))
		return false;
	size_t splitIndex = text.find(' ');
	std::string_view cmdName = splitIndex == std::string_view::npos ? text : text.substr(0, splitIndex);
	ChatCommand* cmd = getCommand(cmdName);
	if (cmd == nullptr)
		return false;
	std::string_view cmdParams = text.substr(cmdName.size());
	std::vector<std::string> params = getParamsFromString(cmdParams);
	return cmd->process(player, params);
}

void ChatProcessor::handleConsoleCommand(model::gameobjects::player::Player& player, std::string_view text) {
	if (isEmpty(text))
		return;
	size_t splitIndex = text.find(' ');
	std::string_view cmdName = splitIndex == std::string_view::npos ? text : text.substr(0, splitIndex);
	auto* consoleCommand = dynamic_cast<ConsoleCommand*>(getCommand(cmdName));
	if (consoleCommand == nullptr) {
		PacketSendUtility::sendMessage(player, "The command " + std::string(cmdName) + " is not implemented.");
		return;
	}
	std::string_view cmdParams = text.substr(cmdName.size());
	std::vector<std::string> params = getParamsFromString(cmdParams);
	consoleCommand->process(player, params);
}

std::vector<std::string> ChatProcessor::getParamsFromString(std::string_view params) {
	std::string_view trimmed = javaTrim(params);
	if (trimmed.empty())
		return {};

	// advanced split to keep item links etc. in one piece (splitting on spaces, but only outside of square brackets)
	// Java: params.trim().split(" +(?=[^\\]]*(\\[|$))"): a run of spaces splits if the text after it reaches a '[' or the end before any ']'
	std::vector<std::string> result;
	size_t start = 0;
	size_t i = 0;
	while (i < trimmed.size()) {
		if (trimmed[i] != ' ') {
			i++;
			continue;
		}
		size_t runEnd = trimmed.find_first_not_of(' ', i);
		size_t closing = trimmed.find(']', runEnd);
		size_t opening = trimmed.find('[', runEnd);
		bool splits = closing == std::string_view::npos || (opening != std::string_view::npos && opening < closing);
		if (splits) {
			result.emplace_back(trimmed.substr(start, i - start));
			start = runEnd;
		}
		i = runEnd;
	}
	result.emplace_back(trimmed.substr(start));
	while (!result.empty() && result.back().empty()) // Java split drops trailing empty strings
		result.pop_back();
	return result;
}

ChatCommand* ChatProcessor::getCommand(std::string_view alias) {
	return commandHandlers.get(commons::utils::StringUtils::toLowerCase(alias));
}

std::vector<ChatCommand*> ChatProcessor::getCommandList() {
	std::vector<ChatCommand*> list;
	for (ChatCommand* command : commandHandlers.values())
		list.push_back(command);
	return list;
}

bool ChatProcessor::isCommandAllowed(model::gameobjects::player::Player& executor, std::string_view alias) {
	return isCommandAllowed(executor, getCommand(alias));
}

bool ChatProcessor::isCommandAllowed(model::gameobjects::player::Player& executor, ChatCommand* command) {
	return command != nullptr && command->validateAccess(executor);
}

bool ChatProcessor::isCommandExists(std::string_view alias) {
	return commandHandlers.containsKey(commons::utils::StringUtils::toLowerCase(alias));
}

ChatProcessor& ChatProcessor::getInstance() {
	static ChatProcessor instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::utils::chathandlers
