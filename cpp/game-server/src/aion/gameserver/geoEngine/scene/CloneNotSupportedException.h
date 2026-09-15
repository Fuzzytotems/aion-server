#pragma once

#include <exception>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::geoEngine::scene {

/**
 * C++ only: java.lang.CloneNotSupportedException, thrown by the scene graph's clone/copyFrom for a child that is neither a Geometry nor a Node
 * (Node.java, DespawnableNode.java) and caught by GeoWorldLoader.
 */
class CloneNotSupportedException : public runtime::Exception {
public:
	using Exception::Exception;
	CloneNotSupportedException() : Exception("") {}
};

} // namespace aion::gameserver::geoEngine::scene
