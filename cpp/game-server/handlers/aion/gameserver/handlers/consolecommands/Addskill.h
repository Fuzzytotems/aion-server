#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/handlers/consolecommands/ConsoleCommandsPrelude.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * ///addskill: gives the target (or the GM) the skill of the given name (the handler data skills.xml) at level 1.
 * <p>
 * C++: the handler-local JAXB classes (SkillData, SkillTemplate) are a record read from the file's DOM (xml::XmlDocument), with the attributes the JAXB classes
 * bind (m5j item 3, owner's decision 2026-10-13); like Java's JAXBUtil.deserialize(file, class) it is read on every use, without a schema.
 */
class Addskill : public ConsoleCommand {
public:
	Addskill();

	void execute(Player& admin, std::span<const std::string> params) override;

	void info(Player& admin, std::optional<std::string_view> message) override;

private:
	/**
	 * Java: the private static class SkillTemplate (int id, name). Its accessor SkillTemplate.getSkillId() and SkillData.getData() are
	 * not ported: the record's fields and the file's records replace them.
	 */
	struct SkillTemplate {
		std::optional<std::string> name; // Java: String name (null when the attribute is missing)
		int32_t id = 0;
	};

	/**
	 * Java: SkillData.getSkillTemplate(name) on JAXBUtil.deserialize(xml, SkillData.class): the first record whose lower-case name equals the
	 * lower-case name
	 */
	static std::optional<SkillTemplate> getSkillTemplate(std::string_view name);
};

} // namespace aion::gameserver::handlers::consolecommands
