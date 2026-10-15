#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/stats/StatsTemplate.xml.h"

namespace aion::gameserver::model::templates::stats {

/**
 * Java com.aionemu.gameserver.model.templates.stats.StatsTemplate. This class is only a container for Stats.
 * <p>
 * C++: Java's PlayerClass.PlayerStatsTemplate (a run-time subclass created per player level by PlayerClass.createStatsTemplate) overrides the
 * speed and base attribute getters, so they are virtual, and so is the destructor, since that subclass is owned through a StatsTemplate handle
 * (header request templates-1). The other getters are not overridden in Java and stay non-virtual.
 *
 * @author Aquanox, Estrayl, Neon
 */
class StatsTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/stats/StatsTemplate.xml.inc"
public:
	virtual ~StatsTemplate() = default;

	virtual float getWalkSpeed() const { return speeds == nullptr ? 0 : speeds->getWalkSpeed(); }

	virtual float getRunSpeed() const { return speeds == nullptr ? 0 : speeds->getRunSpeed(); }

	float getGroupWalkSpeed() const { return speeds == nullptr ? 0 : speeds->getGroupWalkSpeed(); }

	float getRunSpeedFight() const { return speeds == nullptr ? 0 : speeds->getRunSpeedFight(); }

	float getGroupRunSpeedFight() const { return speeds == nullptr ? 0 : speeds->getGroupRunSpeedFight(); }

	virtual float getFlySpeed() const { return speeds == nullptr ? 0 : speeds->getFlySpeed(); }

	int32_t getStunLikeResistance() const { return stunLikeResistance; }

	/** NpcData.init sets it while the templates are loaded (before they are published) */
	void setStunLikeResistance(int32_t value) { stunLikeResistance = value; }

	virtual int32_t getPower() const { return 100; }

	virtual int32_t getHealth() const { return 100; }

	virtual int32_t getAgility() const { return 100; }

	virtual int32_t getBaseAccuracy() const { return 100; }

	virtual int32_t getKnowledge() const { return 100; }

	virtual int32_t getWill() const { return 100; }

private:
	/** Java @XmlTransient int stunLikeResistance (TODO add individual resistance fields when importing PTS template data) */
	int32_t stunLikeResistance = 0;
};

} // namespace aion::gameserver::model::templates::stats
