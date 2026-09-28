#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/model/templates/walker/WalkerTemplate.xml.h"

namespace aion::gameserver::model::templates::walker {

/**
 * Java com.aionemu.gameserver.model.templates.walker.WalkerTemplate.
 * <p>
 * C++: the @XmlTransient `rows` (an int array, null unless the formation is a square) is a C++-only optional list. An absent `rows` attribute is
 * the empty string, Java's null; a present but empty one counts as absent (Java fails the load on Integer.parseInt(""), see
 * docs/deviations/P4-07b.md). `rows=","` gives an empty list, like Java's `",".split(",")`.
 *
 * @author KKnD
 */
class WalkerTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/walker/WalkerTemplate.xml.inc"
private:
	/** Java @XmlTransient int[] rows */
	std::optional<std::vector<int32_t>> rows;

public:
	WalkerTemplate() = default;

	explicit WalkerTemplate(std::string_view routeId);

	/** @throws IndexOutOfBoundsException (Java) for an index outside the steps */
	const RouteStep* getRouteStep(int32_t stepIndex) const;

	/** Java DataManager.WALKER_VERSIONS_DATA.getRouteVersionId(routeId): the route group of a versioned route, nullopt (Java null) otherwise */
	std::optional<std::string> getVersionId() const;

	const std::optional<std::vector<int32_t>>& getRows() const { return rows; }

	void setRows(std::optional<std::vector<int32_t>> value) { this->rows = std::move(value); }
};

} // namespace aion::gameserver::model::templates::walker
