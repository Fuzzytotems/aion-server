#pragma once

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/exceptions/fwd.h"

namespace aion::gameserver::world::exceptions {

/**
 * This exception will be thrown when object attempts to spawn in world, but is already spawned.
 * <p>
 * C++: a runtime exception (Java RuntimeException) whose message is built by the constructor like Java's createMessage.
 *
 * @author -Nemesiss-
 */
class AlreadySpawnedException : public runtime::Exception {
public:
	/** Constructs an AlreadySpawnedException for the given object */
	explicit AlreadySpawnedException(model::gameobjects::VisibleObject& object);

private:
	static std::string createMessage(model::gameobjects::VisibleObject& object);
};

} // namespace aion::gameserver::world::exceptions
