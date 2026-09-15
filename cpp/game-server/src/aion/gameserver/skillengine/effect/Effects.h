#pragma once

#include "aion/gameserver/skillengine/effect/Effects.xml.h"

#include <initializer_list>
#include <set>

#include "aion/gameserver/skillengine/effect/EffectType.h"

namespace aion::gameserver::skillengine::effect {

/**
 * Java com.aionemu.gameserver.skillengine.effect.Effects: the effect list of a skill template.
 * <p>
 * C++ notes (P4-08): the @XmlTransient sets are private members filled by afterUnmarshal; Java's `EnumSet` becomes `std::set` (ordinal
 * iteration order, hub-headers.md §6). `resolveEffectType` maps `javaClassName()` instead of `getClass().getSimpleName()` (static-data.md §2.2).
 * The static helpers `normalizeNoResist` and `resolveEffectType` read no member of this class.
 *
 * @author ATracer
 */
class Effects : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/skillengine/effect/Effects.xml.inc"
private:
	static const std::set<EffectType> CONFLICT_TYPES;
	static const std::set<EffectType> ALWAYS_NO_RESIST;

	/** Java @XmlTransient Set<EffectType> effectTypes: the resolved type of every effect, set by afterUnmarshal */
	std::set<EffectType> effectTypes;
	/** Java @XmlTransient Set<EffectType> possibleConflictEffectTypes = EnumSet.noneOf(EffectType.class) */
	std::set<EffectType> possibleConflictEffectTypes;

	void registerEffectType(EffectType effectType);

	void registerConflictEffectType(EffectType effectType);

	/**
	 * Applies retail-specific normalization for no-resist behavior: retail servers override the XML noresist setting for certain effect types
	 * (mostly beneficial ones, so players cannot resist their own buffs, heals and shields).
	 */
	static void normalizeNoResist(EffectTemplate& effect, EffectType effectType);

	/** @throws IllegalArgumentException "Missing EffectType ..." if the class name does not name a constant */
	static EffectType resolveEffectType(const EffectTemplate& et);

public:
	const std::set<EffectType>& getPossibleConflictEffectTypes() const { return possibleConflictEffectTypes; }

	bool hasAnyEffectType(std::initializer_list<EffectType> types) const;
};

} // namespace aion::gameserver::skillengine::effect
