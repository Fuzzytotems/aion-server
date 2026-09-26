#pragma once

#include <functional>

namespace aion::commons::database {

class ResultSet;

/**
 * Read statement handler.<br>
 * For usage details check documentation of DB class.
 * <p>
 * C++ port: the single method interface handleRead(ResultSet) is a std::function, so a lambda can be passed.
 *
 * @author Disturbing
 */
using ReadStH = std::function<void(ResultSet& rset)>;

} // namespace aion::commons::database
