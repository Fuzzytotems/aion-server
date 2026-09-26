#pragma once

#include <functional>

namespace aion::commons::database {

class PreparedStatement;

/**
 * Insert/Update Statement handler.<br>
 * For usage details check documentation of DB class.
 * <p>
 * C++ port: the single method interface handleInsertUpdate(PreparedStatement) is a std::function, so a lambda can be passed. The handler must
 * execute the statement or batch itself.
 *
 * @author Disturbing
 */
using IUStH = std::function<void(PreparedStatement& stmt)>;

} // namespace aion::commons::database
