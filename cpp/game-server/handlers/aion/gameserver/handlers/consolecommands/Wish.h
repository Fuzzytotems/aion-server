#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///wish: spawns an npc by name, or adds an item by its client name (with a count or an enchant level) to the target.
 * <p>
 * C++: Java's StAX scan (XMLStreamReader over the file) is a read-only streaming scan of the start tags (m5j item 3, owner's decision
 * 2026-10-13): comments, processing instructions, CDATA and declarations are skipped, attribute values are unescaped (the predefined entities
 * and character references), and the file is read in chunks, never as a whole (item_templates.xml is large).
 */
class Wish : public ConsoleCommand {
public:
	Wish();

	void execute(Player& admin, std::span<const std::string> params) override;

private:
	static int32_t findNpcId(std::string_view npcName);

	static int32_t findItemId(std::string_view itemName);

	static int32_t findIdInXml(const std::string& xml, std::string_view elementName, std::string_view attributeName, std::string_view attributeValue);
};

} // namespace aion::gameserver::handlers::consolecommands
