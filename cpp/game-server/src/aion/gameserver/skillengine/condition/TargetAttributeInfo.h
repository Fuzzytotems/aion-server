#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/condition/TargetAttribute.h"

namespace aion::gameserver::skillengine::condition {

/** Companion of the generated enum TargetAttribute (docs/design/static-data.md §2.5): Java's two methods as free functions (ADL). */

/** Java: TargetAttribute.value() - name() */
constexpr std::string_view value(TargetAttribute targetAttribute) noexcept {
	return xml::enumName(targetAttribute);
}

/** Java: TargetAttribute.fromValue(v) - valueOf(v), an IllegalArgumentException for a name that is no constant */
inline TargetAttribute fromValue(std::string_view v) {
	std::optional<TargetAttribute> attribute = xml::enumFromName<TargetAttribute>(v);
	if (!attribute)
		throw runtime::IllegalArgumentException("No enum constant com.aionemu.gameserver.skillengine.condition.TargetAttribute." + std::string(v));
	return *attribute;
}

} // namespace aion::gameserver::skillengine::condition
