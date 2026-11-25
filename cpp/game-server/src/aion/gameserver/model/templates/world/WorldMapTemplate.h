#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/world/WorldMapTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::world {

/**
 * Java com.aionemu.gameserver.model.templates.world.WorldMapTemplate.
 * <p>
 * C++ (upstream sync foundation, docs/deviations/P4-07a.md): upstream 4617c9fd2 and 2b097879f replaced the flags bitmask and its accessors
 * (isFly, canGlide, ..., getFlags) by hasAttribute, and 0199b4b03 removed beginner_twin_count. The C++ callers keep the 51d7a19cb accessors
 * until waves 4 ports those commits: they are C++-only here and read the bound attribute set, so they answer as before; getBeginnerTwinCount
 * is 0 (the data has no beginner channels any more).
 *
 * @author Luno
 */
class WorldMapTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/world/WorldMapTemplate.xml.inc"
public:
	/** @return the name, the cName if the name is absent (Java null) */
	const std::string& getName() const { return name.empty() ? cName : name; }

	int32_t getL10nId() const override { return nameId; }

	/**
	 * @return The number of channels the client lists for this map.
	 */
	int32_t getTwinCount() const;

	bool hasAttribute(::aion::gameserver::world::zone::ZoneAttributes attribute) const { return zoneAttributes.contains(attribute); }

	/** @return the ai_info element, AiInfo::DEFAULT if absent (never null) */
	const AiInfo* getAiInfo() const { return aiInfo != nullptr ? aiInfo.get() : &AiInfo::DEFAULT; }

	// ---- C++ only: the accessors of 51d7a19cb (see the class comment) ----------------------------------------------------------------------
	int32_t getBeginnerTwinCount() const { return 0; }

	bool isFly() const;

	bool canGlide() const;

	bool canPutKisk() const;

	bool canRecall() const;

	bool canRide() const;

	bool canFlyRide() const;

	bool isPvpAllowed() const;

	bool isSameRaceDuelsAllowed() const;

	bool isOtherRaceDuelsAllowed() const;

	bool canReturnToBattle() const;

	/** the ZoneAttributes ids of the bound attributes (Java 51d7a19cb: ZoneAttributes.fromList(flagValues)) */
	int32_t getFlags() const;
};

} // namespace aion::gameserver::model::templates::world
