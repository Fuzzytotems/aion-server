#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::utils::chathandlers {

/**
 * Base of all chat commands (admin, player and console commands).
 * <p>
 * Hub header (docs/design/hub-headers.md). Commands are process-lifetime singletons (handlers-and-porting-plan.md amendment §2): the command
 * registry (HandlerRegistry.h CommandFactory) creates each one with `std::make_unique<C>()` and ChatProcessor keeps it; retired commands are
 * never freed (RT-11). Hence runtime::Immortal (fieldmap.json names RefCounted because admincommands.Speed implements StatOwner; such a command
 * implements StatOwner's retain()/release() as no-ops) and a public virtual destructor.
 * <p>
 * C++ signatures: the command parameters (Java `String... params`, always an array split from the chat line) are
 * `std::span<const std::string>`; `sendInfo(player, "a", "b")` keeps its varargs syntax through a forwarding template (zero messages send
 * the syntax info, like Java's empty or null message). toErrorMessage returns std::nullopt where Java returns null. The constructor is ported
 * (parseSyntaxInfo only formats text), so every command can be created before the command bodies are ported.
 *
 * @author KID, Neon
 */
// lint: L13 commands are Immortal (amendment §2); L13 accepts their subclasses but not the root class
class ChatCommand : public runtime::Immortal {
private:
	const std::string prefix;
	const std::string alias;
	const std::string description;
	const std::string syntaxInfo;

public:
	/**
	 * Initializes a chat command.
	 *
	 * @param prefix
	 *          prefix for this command
	 * @param alias
	 *          command identifier
	 * @param description
	 *          short command description
	 * @param syntaxInfo
	 *          The command parameter info. It is used to generate the syntax info in {@link #sendInfo(Player, String...)}.<br>
	 *          When following the parameter convention, parameters will be highlighted in white. You can pass a text block if your command
	 *          supports multiple syntax variants.<br>
	 *          Example:
	 *          <pre>{@code
	 *          - Short description for no parameter.
	 *          <param1> <param2> [optionalParam3] - Short parameter description (two mandatory parameters, third one is optional).
	 *          param1 <param2> - Short parameter description (first one is a non-variable word).
	 *          Some other help text.
	 *          }</pre>
	 */
	ChatCommand(std::string_view prefix, std::string_view alias, std::string_view description, std::string_view syntaxInfo);

	/** Commands are owned by ChatProcessor through std::unique_ptr<ChatCommand> (HandlerRegistry.h CommandFactory). */
	virtual ~ChatCommand();

	bool run(model::gameobjects::player::Player& player, std::span<const std::string> params);

	std::string getPrefix() const { return prefix; }

	std::string getAlias() const { return alias; }

protected:
	virtual std::string getAliasForLevel() { return alias; }

public:
	std::string getDescription() const { return description; }

	std::string getAliasWithPrefix() const { return prefix + alias; }

	virtual std::string getSyntaxInfo() { return syntaxInfo; }

private:
	std::string parseSyntaxInfo(std::string_view syntaxInfo);

public:
	int8_t getLevel();

	/**
	 * @param player
	 * @return True if player is allowed to use this command.
	 */
	virtual bool validateAccess(model::gameobjects::player::Player& player) = 0;

	/**
	 * Handles processing of a chat command.
	 *
	 * @param player
	 * @param params
	 * @return True if command was executed.
	 */
	virtual bool process(model::gameobjects::player::Player& player, std::span<const std::string> params) = 0;

protected:
	/**
	 * The code to be executed after successful command access validation. Any IllegalArgumentException and its subclasses will be catched,
	 * printing the error message to the player (using {@link #toErrorMessage(IllegalArgumentException)}).
	 */
	virtual void execute(model::gameobjects::player::Player& player, std::span<const std::string> params) = 0;

	/**
	 * This method can be overridden in case the default message extraction is not sufficient.
	 * <p>
	 * C++: non-virtual (hub-headers.md §9.1), since no command in src/ or data/handlers overrides it; a command that needs its own extraction
	 * files an additive header request.
	 *
	 * @return Message that should be sent to the player who caused the exception with invalid input. If std::nullopt (Java: null), the default
	 *         syntax info will be sent, as specified by {@link #sendInfo(Player, String...)}
	 */
	std::optional<std::string> toErrorMessage(const runtime::IllegalArgumentException& e);

	/**
	 * Sends an info message to the player.<br>
	 * If no message parameter is specified, the default syntax info will be sent.
	 *
	 * @param player
	 *          player who will receive the message
	 * @param message
	 *          message text (insert newlines with \n or by passing comma separated strings)
	 */
	void sendInfo(model::gameobjects::player::Player& player, std::span<const std::string> message);

	/** C++ only: Java's varargs call syntax `sendInfo(player, "line 1", text)`; `sendInfo(player)` sends the syntax info. */
	template <class... Message>
		requires(std::convertible_to<Message, std::string_view> && ...)
	void sendInfo(model::gameobjects::player::Player& player, Message&&... message) {
		std::vector<std::string> lines;
		lines.reserve(sizeof...(message));
		((void)lines.emplace_back(std::string_view(message)), ...);
		sendInfo(player, std::span<const std::string>(lines));
	}

	static std::string join(std::span<const std::string> params, int32_t startIndex);

	/**
	 * @return The name of the object to be displayed in chat. If the object is a player, a clickable name will be returned. Otherwise, it's a
	 *         localized name if available.
	 */
	static std::string name(model::gameobjects::VisibleObject& visibleObject);

	/**
	 * @return The name of the world to be displayed in chat. If available, returns its localized name.
	 */
	static std::string worldName(int32_t worldId);

	/**
	 * Please use {@link #sendInfo(Player, String...)}.
	 * Old commands still override this method to show syntax info and should be ported eventually.
	 * (Java: @Deprecated; remove this method when all commands are updated)
	 * <p>
	 * C++: `message` is std::optional<std::string_view> (hub-headers.md §6): 29 legacy call sites pass null and the overrides distinguish it
	 * from text (Remove.java checks `message != null`, BanMac.java dereferences it). Java `info(player, null)` is `info(player, std::nullopt)`.
	 */
	virtual void info(model::gameobjects::player::Player& player, std::optional<std::string_view> message);
};

} // namespace aion::gameserver::utils::chathandlers
