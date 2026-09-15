#pragma once

#include <string>
#include <vector>

#include "aion/gameserver/model/templates/tribe/Tribe.xml.h"

namespace aion::gameserver::model::templates::tribe {

/** Java com.aionemu.gameserver.model.templates.tribe.Tribe. C++: an absent relation list is empty. @author ATracer */
class Tribe : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/tribe/Tribe.xml.inc"
public:
	/** Java replaces an absent list by Collections.emptyList() on first use (a write to the template) */
	const std::vector<TribeClass>& getAggro() const { return listOrEmpty(aggro); }

	const std::vector<TribeClass>& getHostile() const { return listOrEmpty(hostile); }

	const std::vector<TribeClass>& getFriend() const { return listOrEmpty(friend_); }

	const std::vector<TribeClass>& getNeutral() const { return listOrEmpty(neutral); }

	const std::vector<TribeClass>& getNone() const { return listOrEmpty(none); }

	const std::vector<TribeClass>& getSupport() const { return listOrEmpty(support); }

	/** @return the base tribe, or the tribe itself without one */
	TribeClass getBase() const { return base == TribeClass::NONE ? name : base; }

	bool isGuard() const;

	/** Java: name + " (" + base + ")" */
	std::string toString() const;

private:
	static const std::vector<TribeClass>& listOrEmpty(const std::optional<std::vector<TribeClass>>& list);
};

} // namespace aion::gameserver::model::templates::tribe
