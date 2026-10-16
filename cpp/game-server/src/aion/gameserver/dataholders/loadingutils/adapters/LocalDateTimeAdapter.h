#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include "aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h"

namespace aion::gameserver::xml::adapters {

/** java.time.LocalDateTime with the precision static data uses (docs/design/static-data.md §2.4). */
using LocalDateTime = std::chrono::local_time<std::chrono::milliseconds>;

/**
 * Java: LocalDateTimeAdapter.unmarshal = LocalDateTime.parse(v) (DateTimeFormatter.ISO_LOCAL_DATE_TIME, STRICT resolver), @author Neon.
 * Accepted: `yyyy-MM-dd'T'HH:mm[:ss[.fraction]]`, 'T' in either case, a fraction of 0-9 digits after the '.', month/day/hour/minute/second
 * validated (leap years included). No whitespace is trimmed.
 * Stricter than Java (proven unused by the census): years must have exactly 4 digits without sign, and sub-millisecond fraction digits must
 * be zero (LocalDateTime keeps nanoseconds).
 * @throws XmlValueException
 */
LocalDateTime parseLocalDateTime(std::string_view value);
/** Same, with the location of the attribute or element being bound on errors. @throws StaticDataException */
LocalDateTime parseLocalDateTime(BindContext& context, std::string_view value);
/** Java: LocalDateTime.toString (LocalDateTimeAdapter.marshal): seconds only if non-zero or a fraction exists, fraction as 3 digits. */
std::string printLocalDateTime(LocalDateTime value);

} // namespace aion::gameserver::xml::adapters
