#pragma once

/**
 * All transformers available to ConfigurableProcessor. Supported field types (more can be added by specializing PropertyTransformer, see
 * PropertyTransformer.h):
 * <ul>
 * <li>integers: int8_t, int16_t, int32_t, int64_t (Java Byte/Short/Integer/Long.decode) and the unsigned types</li>
 * <li>float, double (Java Float/Double.valueOf)</li>
 * <li>bool (true/false case-insensitive, 1/0)</li>
 * <li>char16_t (Java char)</li>
 * <li>std::string</li>
 * <li>enums (magic_enum, case-sensitive names)</li>
 * <li>std::vector, std::set and std::unordered_set of supported types (Java arrays, List, Set: comma separated values)</li>
 * <li>std::filesystem::path (Java File)</li>
 * <li>utils::InetSocketAddress ("host:port")</li>
 * <li>std::regex, std::wregex (Java Pattern)</li>
 * <li>const std::chrono::time_zone* (Java ZoneId)</li>
 * <li>std::optional of supported types (empty value = std::nullopt, for fields that are null in Java)</li>
 * <li>maps with supported key and value types, for ConfigurableProcessor::bindPattern only</li>
 * </ul>
 * ConfigurableProcessor also binds ConfigValue&lt;T&gt; and std::atomic&lt;T&gt; of these types, for fields that are rebound while other threads
 * read them.
 * Not ported: ClassTransformer (Java Class&lt;?&gt; fields, resolved by reflection; no config uses it) and TimeZoneTransformer (no config uses it).
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.PropertyTransformers
 */

#include "aion/commons/configuration/transformers/BooleanTransformer.h"
#include "aion/commons/configuration/transformers/CharTransformer.h"
#include "aion/commons/configuration/transformers/CollectionTransformer.h"
#include "aion/commons/configuration/transformers/EnumTransformer.h"
#include "aion/commons/configuration/transformers/FileTransformer.h"
#include "aion/commons/configuration/transformers/InetSocketAddressTransformer.h"
#include "aion/commons/configuration/transformers/MapTransformer.h"
#include "aion/commons/configuration/transformers/NumberTransformer.h"
#include "aion/commons/configuration/transformers/OptionalTransformer.h"
#include "aion/commons/configuration/transformers/PatternTransformer.h"
#include "aion/commons/configuration/transformers/PropertyTransformer.h"
#include "aion/commons/configuration/transformers/StringTransformer.h"
#include "aion/commons/configuration/transformers/ZoneIdTransformer.h"
