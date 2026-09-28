#include "aion/gameserver/skillengine/effect/Effects.h"

#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::skillengine::effect {

using enum EffectType;

const std::set<EffectType> Effects::CONFLICT_TYPES{SHIELD, PROTECT, REFLECTOR, MPSHIELD};

const std::set<EffectType> Effects::ALWAYS_NO_RESIST{ABSOLUTESTATTOPCBUFF, ALWAYSBLOCK, ALWAYSDODGE, ALWAYSPARRY, ALWAYSRESIST, ARMORMASTERY, APBOOST,
	AURA, BOOSTHATE, BOOSTHEAL, BOOSTSKILLCASTINGTIME, BOOSTSKILLCOST, BOOSTSPELLATTACK, CASEHEAL, CHANGEHATEONATTACKED, CONDSKILLLAUNCHER, CONVERTHEAL,
	DISPELDEBUFF, DISPELDEBUFFMENTAL, DISPELDEBUFFPHYSICAL, DISPELNPCDEBUFF, DPHEAL, DPHEALINSTANT, DPTRANSFER, DRBOOST, ESCAPE, EXTENDAURARANGE, FPHEAL,
	FPHEALINSTANT, HEAL, HEALINSTANT, HIDE, HIPASS, HOSTILEUP, INVULNERABLEWING, SKILLXPBOOST, MPHEAL, MPHEALINSTANT, MPSHIELD, NODEATHPENALTY,
	NORESURRECTPENALTY, ONETIMEBOOSTHEAL, ONETIMEBOOSTSKILLATTACK, ONETIMEBOOSTSKILLCRITICAL, PETORDERUSEULTRASKILL, POLYMORPH, PROCDPHEALINSTANT,
	PROCFPHEALINSTANT, PROCHEALINSTANT, PROCMPHEALINSTANT, PROCVPHEALINSTANT, PROTECT, RANDOMMOVELOC, REBIRTH, RECALLINSTANT, REFLECTOR, RESURRECT,
	RESURRECTBASE, RESURRECTPOSITIONAL, RETURN, RETURNPOINT, RIDEROBOT, SANCTUARY, SEARCH, SHAPECHANGE, SHIELD, SHIELDMASTERY, SIGNET, SKILLLAUNCHER,
	STATBOOST, STATUP, SUBTYPEBOOSTRESIST, SUBTYPEEXTENDDURATION, SUMMON, SUMMONBINDINGGROUPGATE, SUMMONFUNCTIONALNPC, SUMMONGROUPGATE, SUMMONHOMING,
	SUMMONHOUSEGATE, SUMMONSERVANT, SUMMONSKILLAREA, SUMMONTOTEM, SUMMONTRAP, SUPPORTEVENT, SWITCHHOSTILE, SWITCHHPMP, WEAPONSTATBOOST, WEAPONSTATUP,
	WEAPONDUAL, WEAPONMASTERY, XPBOOST};

void Effects::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	// Java iterates the bound list, which is null for an <effects> element without children: NullPointerException (no such element in the data)
	if (effects.empty())
		ctx.fail("java.lang.NullPointerException: Effects.effects is null (an <effects> element without effects)");
	effectTypes.clear(); // Java: effectTypes = EnumSet.noneOf(EffectType.class)

	for (const std::unique_ptr<EffectTemplate>& effect : effects) {
		EffectType effectType = resolveEffectType(*effect);

		registerEffectType(effectType);
		registerConflictEffectType(effectType);
		normalizeNoResist(*effect, effectType);
	}
}

void Effects::registerEffectType(EffectType effectType) {
	effectTypes.insert(effectType);
}

void Effects::registerConflictEffectType(EffectType effectType) {
	if (CONFLICT_TYPES.contains(effectType))
		possibleConflictEffectTypes.insert(effectType);
}

void Effects::normalizeNoResist(EffectTemplate& effect, EffectType effectType) {
	if (ALWAYS_NO_RESIST.contains(effectType))
		effect.setNoResist(true);
}

EffectType Effects::resolveEffectType(const EffectTemplate& et) {
	// Java: et.getClass().getSimpleName().replace("Effect", "").toUpperCase(); class names are ASCII
	std::string_view simpleName = et.javaClassName();
	std::string effectName;
	for (size_t pos = 0; pos < simpleName.size();) {
		if (simpleName.substr(pos).starts_with("Effect")) {
			pos += 6;
			continue;
		}
		char c = simpleName[pos++];
		effectName += (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
	}
	if (std::optional<EffectType> type = xml::enumFromName<EffectType>(effectName))
		return *type;
	// Java: "... for: " + et.getClass(), which prints "class <FQN>"
	throw runtime::IllegalArgumentException("Missing EffectType " + effectName + " for: class com.aionemu.gameserver.skillengine.effect." +
		std::string(simpleName));
}

bool Effects::hasAnyEffectType(std::initializer_list<EffectType> types) const {
	for (EffectType effectType : types) {
		if (effectTypes.contains(effectType))
			return true;
	}
	return false;
}

} // namespace aion::gameserver::skillengine::effect
