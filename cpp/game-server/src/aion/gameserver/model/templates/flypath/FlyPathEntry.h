#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/flypath/FlyPathEntry.xml.h"

#include "aion/gameserver/model/geometry/fwd.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::templates::flypath {

/**
 * Java com.aionemu.gameserver.model.templates.flypath.FlyPathEntry (windstream points: upstream 332d54885). Java's null windstream point
 * list is the empty bound vector: isWindstream is false without points (the data has no present-empty list).
 *
 * @author KID
 */
class FlyPathEntry : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/flypath/FlyPathEntry.xml.inc"
public:
	/** Java: (int) (time * 1000) */
	int32_t getTimeInMs() const { return templates::detail::floatToInt(time * 1000.0f); }

	bool isWindstream() const { return !windstreamPoints.empty(); }

	/**
	 * @param progress
	 *          flight progress in milliseconds, as sent by the client
	 * @return The interpolated position on the windstream path, or the end point if the progress exceeds the path.
	 */
	runtime::Ref<geometry::Point3D> getWindstreamPosition(int32_t progress) const;

	/**
	 * @return True if the path may be entered at the given progress (in milliseconds), which is the case if the point of this or the next second
	 *         allows it, or the progress is within the last second of the path.
	 */
	bool canEnterWindstream(int32_t progress) const;

	/**
	 * @return True if the path may be left at the given progress (in milliseconds), which is the case if the point of this or the next second
	 *         allows it, or the progress is within the last second of the path.
	 */
	bool canLeaveWindstream(int32_t progress) const;
};

} // namespace aion::gameserver::model::templates::flypath
