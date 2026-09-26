#include "aion/gameserver/model/templates/flyring/FlyRingPoint.h"

#include "aion/gameserver/model/geometry/Point3D.h"

namespace aion::gameserver::model::templates::flyring {

FlyRingPoint::FlyRingPoint(geometry::Point3D& p) : x(p.getX()), y(p.getY()), z(p.getZ()) {}

} // namespace aion::gameserver::model::templates::flyring
