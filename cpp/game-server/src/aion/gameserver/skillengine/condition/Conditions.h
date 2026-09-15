#pragma once

#include "aion/gameserver/skillengine/condition/Conditions.xml.h"

#include <memory>
#include <vector>

namespace aion::gameserver::skillengine::condition {

/**
 * Java com.aionemu.gameserver.skillengine.condition.Conditions.
 * <p>
 * C++ notes (P4-08): `getConditions()` returns the bound list, which is empty where Java's is null (Java then returns an empty list). The
 * validate/canValidate families are behaviour (P5-02).
 *
 * @author ATracer
 */
class Conditions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/condition/Conditions.xml.inc"
public:
	const std::vector<std::unique_ptr<Condition>>& getConditions() const { return conditions; }
};

} // namespace aion::gameserver::skillengine::condition
