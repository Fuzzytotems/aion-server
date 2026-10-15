#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///deleteskill: removes the skill of the given name from the target (or the GM), unless it is a stigma skill.
 * <p>
 * C++: the handler-local JAXB classes (SkillData, SkillTemplate) are a record read from the file's DOM (xml::XmlDocument), with the attributes the JAXB classes
 * bind (m5j item 3, owner's decision 2026-10-13); like Java's JAXBUtil.deserialize(file, class) it is read on every use, without a schema.
 */
class Deleteskill : public ConsoleCommand {
public:
	Deleteskill();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& admin, std::optional<std::string_view> message) override;

private:
	/** Java: the private static class SkillTemplate (String id (@XmlID, parsed in afterUnmarshal), name) */
	struct SkillTemplate {
		std::optional<std::string> name; // Java: String name (null when the attribute is missing)
		int32_t id = 0;
	};

	/** Java: JAXBUtil.deserialize(xml, SkillData.class).getSkillTemplate(name): the first record whose lower-case name equals the lower-case name */
	static std::optional<SkillTemplate> getSkillTemplate(std::string_view name);

	static bool check(Player& admin, Player& player, int32_t skillId);

	void apply(Player& admin, Player& player, int32_t skillId);
};

} // namespace aion::gameserver::handlers::consolecommands
