#pragma once

#include "aion/commons/utils/Exception.h"

namespace aion::gameserver::xml {

/**
 * Error while resolving, parsing or binding static data (Java: GameServerError "Error while loading static data" wrapping JAXB's
 * UnmarshalException from XmlValidationHandler). Messages carry `file:line:col: element/path: problem` where a location is known. Startup
 * aborts on it, as with JAXB.
 */
class StaticDataException : public commons::utils::Exception {
public:
	using Exception::Exception;
};

} // namespace aion::gameserver::xml
