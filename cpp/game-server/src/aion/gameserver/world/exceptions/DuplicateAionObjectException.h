#pragma once

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/world/exceptions/fwd.h"

namespace aion::gameserver::world::exceptions {

/**
 * This Exception will be thrown when some AionObject will be stored more then one time. This Exception indicating serious error.
 * <p>
 * C++: a runtime exception (Java RuntimeException). Java passes `map.get(id)` as the present object, which can be null when the entry was
 * removed concurrently; that argument is therefore a Ptr and formatted as "null".
 *
 * @author -Nemesiss-
 */
class DuplicateAionObjectException : public runtime::Exception {
public:
	/** Constructs an DuplicateAionObjectException for the given objects */
	DuplicateAionObjectException(model::gameobjects::AionObject& object, runtime::Ptr<model::gameobjects::AionObject> presentObject);

private:
	static std::string createMessage(model::gameobjects::AionObject& object, runtime::Ptr<model::gameobjects::AionObject> presentObject);
};

} // namespace aion::gameserver::world::exceptions
