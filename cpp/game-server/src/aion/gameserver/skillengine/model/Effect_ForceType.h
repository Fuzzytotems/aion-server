#pragma once

#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::skillengine::model {

/**
 * Java com.aionemu.gameserver.skillengine.model.Effect.ForceType: interned force type identifiers (fieldmap.toml [immortal]), referenced as
 * `const Effect_ForceType*` and compared by identity like Java.
 * <p>
 * C++: the static nested class is hoisted to namespace scope under the nested-enum spelling `Outer_Inner` (Effect.h declares
 * `using ForceType = Effect_ForceType;`, so `Effect::ForceType` keeps working), because a nested class cannot be forward declared: headers that
 * only pass force types around (SkillEngine.h) include this lean header instead of the Effect.h hub. The definitions live in Effect.cpp (the
 * Java file). DEFAULT and MATERIAL_SKILL are interned during static initialization (ported getInstance).
 */
// fieldmap-class: com.aionemu.gameserver.skillengine.model.Effect.ForceType
class Effect_ForceType : public runtime::Immortal {
private:
	static inline runtime::ConcurrentHashMap<std::string, const Effect_ForceType*> forceTypes{AION_LOCK_CLASS(Effect::ForceType::forceTypes#stripe)};

public:
	static const Effect_ForceType* const DEFAULT;
	static const Effect_ForceType* const MATERIAL_SKILL;

private:
	const std::string name;

	explicit Effect_ForceType(std::string_view name);

public:
	std::string getName() const { return name; }

	static const Effect_ForceType* getInstance(std::string_view name);
};

} // namespace aion::gameserver::skillengine::model
