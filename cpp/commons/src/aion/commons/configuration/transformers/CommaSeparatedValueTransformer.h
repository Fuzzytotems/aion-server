#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace aion::commons::configuration::transformers::CommaSeparatedValueTransformer {

/**
 * Modified version of http://stackoverflow.com/a/24078092<br>
 * Splits strings on every comma outside quotes.<br>
 * Example: {@code a,b,"c,d", e , " f " = ["a", "b", "c,d", "e", " f "]}
 * <p>
 * Every token is trimmed (characters &lt;= ' '), then one leading and one trailing quote are removed if both are present. The last token is dropped
 * if it is empty, so "" yields no tokens and "a," yields ["a"], while ",a" yields ["", "a"].
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.CommaSeparatedValueTransformer.splitAndTrimValues
 *
 * @return List of trimmed strings, separated by comma. Leading+trailing quotes are removed if both were present.
 * @author Neon
 */
std::vector<std::string> splitAndTrimValues(std::string_view value);

} // namespace aion::commons::configuration::transformers::CommaSeparatedValueTransformer
