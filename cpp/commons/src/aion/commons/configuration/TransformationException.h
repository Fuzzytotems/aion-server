#pragma once

#include "aion/commons/utils/Exception.h"

namespace aion::commons::configuration {

/**
 * This exception indicates errors while transforming parsed configuration values to actual field values (according to the bindings made with
 * ConfigurableProcessor). The cause, if any, is the underlying parse error.
 * <p>
 * Java: com.aionemu.commons.configuration.TransformationException
 *
 * @author SoulKeeper
 */
class TransformationException : public utils::Exception {
public:
	using utils::Exception::Exception;
};

} // namespace aion::commons::configuration
