#pragma once

#include <optional>

#include "aion/gameserver/questEngine/handlers/HandlerResult.h"

namespace aion::gameserver::questEngine::handlers {

/**
 * Companion of the generated enum HandlerResult (docs/design/static-data.md §2.5): Java's static method as a free function
 * (`fromBoolean(b)` for Java `HandlerResult.fromBoolean(b)`).
 * <p>
 * The file name, the namespace and the signature are m5d-plan.md D17(a) (header request m5d-h02): the quest transliterator
 * (phase6-questgen-prototype.md §5.2 item 1, tools/gen/questgen/api.py PLANNED) emits
 * `::aion::gameserver::questEngine::handlers::fromBoolean(b)` in 60 generated handlers, and including this header.
 */

/** Java: HandlerResult.fromBoolean(Boolean) - UNKNOWN for null, SUCCESS for true, FAILED for false (Java Boolean: std::optional<bool>) */
HandlerResult fromBoolean(std::optional<bool> value);

} // namespace aion::gameserver::questEngine::handlers
