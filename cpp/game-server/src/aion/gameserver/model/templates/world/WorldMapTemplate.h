#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/world/WorldMapTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::world {

/** Java com.aionemu.gameserver.model.templates.world.WorldMapTemplate. @author Luno */
class WorldMapTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/world/WorldMapTemplate.xml.inc"
public:
	/** @return the name, the cName if the name is absent (Java null) */
	const std::string& getName() const { return name.empty() ? cName : name; }

	int32_t getL10nId() const override { return nameId; }

	/** @return the twin count, limited by WorldConfig.WORLD_MAX_TWINS_USUAL (0: no limit) */
	int32_t getTwinCount() const;

	/** @return the beginner twin count, limited by WorldConfig.WORLD_MAX_TWINS_BEGINNER (0: no limit, -1: disabled) */
	int32_t getBeginnerTwinCount() const;

	/* Default zone attributes for the map */
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

	int32_t getFlags() const { return flags; }

	/** @return the ai_info element, AiInfo::DEFAULT if absent (never null) */
	const AiInfo* getAiInfo() const { return aiInfo != nullptr ? aiInfo.get() : &AiInfo::DEFAULT; }

private:
	/** Java @XmlTransient int flags: the ZoneAttributes ids of `flagValues`, computed by afterUnmarshal */
	int32_t flags = 0;
};

} // namespace aion::gameserver::model::templates::world
