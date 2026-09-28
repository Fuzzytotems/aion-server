#pragma once

#include <cstdint>

namespace aion::gameserver::model::templates::item {

/**
 * Java com.aionemu.gameserver.model.templates.item.AssembledItem: a JAXB type that no unmarshal root reaches (xmlgen-report.md "Unreachable
 * JAXB-annotated types"), so nothing binds it; a value class (fieldmap K5).
 *
 * @author rolandas
 */
class AssembledItem {
private:
	int32_t id = 0;

public:
	int32_t getId() const { return id; }
};

} // namespace aion::gameserver::model::templates::item
