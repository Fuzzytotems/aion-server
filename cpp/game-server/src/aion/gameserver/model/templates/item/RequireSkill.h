#pragma once

#include <cstdint>
#include <vector>

namespace aion::gameserver::model::templates::item {

/**
 * Java com.aionemu.gameserver.model.templates.item.RequireSkill: a JAXB type that no unmarshal root reaches (xmlgen-report.md "Unreachable
 * JAXB-annotated types"), so nothing binds it; a value class (fieldmap K5). Java creates the list on first use; the C++ vector always exists.
 */
class RequireSkill {
protected:
	std::vector<int32_t> skillIds;

public:
	std::vector<int32_t>& getSkillIds() { return skillIds; }
};

} // namespace aion::gameserver::model::templates::item
