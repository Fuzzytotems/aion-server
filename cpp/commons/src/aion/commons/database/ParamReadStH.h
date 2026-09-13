#pragma once

#include <functional>

#include "aion/commons/database/ReadStH.h"

namespace aion::commons::database {

class PreparedStatement;

/**
 * Read statement handler.<br>
 * Allows to set query params before execution.<br>
 * For usage details check documentation of DB class.
 * <p>
 * C++ port: the two methods of the Java interface are two std::function members, so the Java anonymous classes become
 * <pre>
 * DB::select(query, ParamReadStH{
 * 	.setParams = [&](PreparedStatement& stmt) { stmt.setInt(1, id); },
 * 	.handleRead = [&](ResultSet& rset) { ... }});
 * </pre>
 * (or the shorter DB::select(query, setParams, handleRead) overload).
 *
 * @author Disturbing
 */
struct ParamReadStH {
	/** Enables coder to manually modify statement parameters. May be empty. */
	std::function<void(PreparedStatement& stmt)> setParams;
	/** Allows coder to read data after query execution. */
	ReadStH handleRead;
};

/**
 * Java: CallReadStH (stored procedure call handler, @author ATracer). Stored procedures are called through PreparedStatement, so it has the same
 * shape as ParamReadStH.
 */
using CallReadStH = ParamReadStH;

} // namespace aion::commons::database
