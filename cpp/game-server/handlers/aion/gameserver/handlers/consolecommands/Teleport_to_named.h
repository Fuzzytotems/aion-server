#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///teleport_to_named: teleports the GM to the npc of the given name (the handler data npcs.xml).
 * <p>
 * C++: the handler-local JAXB classes (NpcData, NpcTemplate) are a record read from the file's DOM (xml::XmlDocument), with the attributes the JAXB classes
 * bind (m5j item 3, owner's decision 2026-10-13); like Java's JAXBUtil.deserialize(file, class) it is read on every use, without a schema.
 */
class Teleport_to_named : public ConsoleCommand {
public:
	Teleport_to_named();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& admin, std::optional<std::string_view> message) override;

private:
	/**
	 * Java: the private static class NpcTemplate (id, name). Its accessors NpcTemplate.getTemplateId() and NpcTemplate.setNpcId(int), its
	 * afterUnmarshal(Unmarshaller, Object) and NpcData.getData() are not ported: the record's parsed id and the file's records replace them.
	 */
	struct NpcTemplate {
		std::optional<std::string> name; // Java: String name (null when the attribute is missing)
		int32_t id = 0;
	};

	/**
	 * Java: NpcData.getNpcTemplate(name) on JAXBUtil.deserialize(xml, NpcData.class): the first record whose lower-case name equals the
	 * lower-case name
	 */
	static std::optional<NpcTemplate> getNpcTemplate(std::string_view name);
};

} // namespace aion::gameserver::handlers::consolecommands
