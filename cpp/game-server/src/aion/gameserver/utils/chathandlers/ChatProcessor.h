#pragma once

#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

#include "aion/gameserver/model/GameEngine.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/utils/chathandlers/fwd.h"

namespace aion::gameserver::utils::chathandlers {

/**
 * Owns the chat commands (admin, player and console commands) and dispatches chat lines and console commands to them.
 * <p>
 * C++ notes:
 * - Java's ScriptManager with ChatCommandsLoader (runtime compilation of data/handlers) is replaced by the command registry (HandlerRegistry.h
 *   commandEntries(), handlers-and-porting-plan.md §1.6): init() creates every registered command whose handler directory
 *   (CommandKind: admincommands, playercommands, consolecommands) is listed in CommandsConfig.HANDLER_DIRECTORIES, in registry order.
 * - Commands are Immortal (RT-11): the registry factory's std::unique_ptr is released and commandHandlers holds plain pointers to commands that
 *   are never freed, also after reload() (like QuestEngine.questHandlers).
 * - getCommand(Class<T>) is the member template getCommand<T>() (a dynamic_cast finds the exact command class).
 *
 * @author KID, Rolandas, Neon
 */
class ChatProcessor : public runtime::Immortal, public model::GameEngine {
private:
	// fieldmap: commands are Immortal and never freed (RT-11, class comment): plain pointers like QuestEngine.questHandlers (fieldmap.toml decision requested)
	runtime::HashMap<std::string, ChatCommand*> commandHandlers{AION_LOCK_CLASS(ChatProcessor::commandHandlers)};

	ChatProcessor();
	~ChatProcessor();

	/** C++ only: the unit tests reach the private parameter split */
	friend struct ChatProcessorTestAccess;

public:
	void init() override;

	void reload();

	void registerCommand(ChatCommand& cmd);

	bool handleChatCommand(model::gameobjects::player::Player& player, std::string_view text);

	void handleConsoleCommand(model::gameobjects::player::Player& player, std::string_view text);

private:
	std::vector<std::string> getParamsFromString(std::string_view params);

	ChatCommand* getCommand(std::string_view alias);

public:
	/**
	 * @return Command of the given type. Java: getCommand(Class<T> commandType) - the command whose runtime class is exactly T, null if none.
	 */
	template <class T>
	T* getCommand() {
		for (ChatCommand* command : getCommandList()) {
			if (typeid(*command) == typeid(T))
				return static_cast<T*>(command);
		}
		return nullptr;
	}

	std::vector<ChatCommand*> getCommandList();

	bool isCommandAllowed(model::gameobjects::player::Player& executor, std::string_view alias);

	bool isCommandAllowed(model::gameobjects::player::Player& executor, ChatCommand* command);

	bool isCommandExists(std::string_view alias);

	static ChatProcessor& getInstance();
};

} // namespace aion::gameserver::utils::chathandlers
