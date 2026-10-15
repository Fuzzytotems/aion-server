#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * Access to the player_transfers table. Thread safe.
 * <p>
 * Java: com.aionemu.loginserver.dao.PlayerTransferDAO
 * <p>
 * PlayerTransferTask belongs to the player transfer service (not to the data layer), so the functions that take or return it are templates:
 * <code>PlayerTransferDAO::getNew&lt;PlayerTransferTask&gt;()</code>, <code>PlayerTransferDAO::update(task)</code>. The task type must be
 * default constructible and have the public fields and constants of the Java class: <tt>int32_t id, sourceAccountId, targetAccountId,
 * playerId; int8_t sourceServerId, targetServerId, status;</tt> a <tt>comment</tt> of type std::optional&lt;std::string&gt; (Java: nullable
 * String) or std::string, and <tt>STATUS_WAIT</tt>, <tt>STATUS_ACTIVE</tt>, <tt>STATUS_DONE</tt>, <tt>STATUS_ERROR</tt> (0-3).
 *
 * @author KID
 */
namespace aion::loginserver::dao::PlayerTransferDAO {

/** Java: PlayerTransferTask.STATUS_* */
inline constexpr int8_t STATUS_WAIT = 0, STATUS_ACTIVE = 1, STATUS_DONE = 2, STATUS_ERROR = 3;

/** The columns of a new transfer task read by getNew. */
struct NewTask {
	int32_t id = 0;
	int8_t sourceServerId = 0;
	int8_t targetServerId = 0;
	int32_t sourceAccountId = 0;
	int32_t targetAccountId = 0;
	int32_t playerId = 0;
};

/**
 * C++ helper for getNew: reads all tasks with status STATUS_WAIT and passes each to the consumer. Errors are logged ("Can't select getNew: ");
 * the tasks read before the error have been passed to the consumer.
 */
void forEachNewTask(const std::function<void(const NewTask& task)>& consumer);

/**
 * C++ helper for update(task): sets status and comment (std::nullopt: NULL) of the task with the given id, plus time_performed=NOW() for
 * STATUS_ACTIVE or time_done=NOW() for STATUS_DONE and STATUS_ERROR.
 *
 * @return true if the query ran successfully
 */
bool update(int32_t id, int8_t status, std::optional<std::string_view> comment);

/** @return the new tasks (status STATUS_WAIT); on errors the tasks read before the error */
template <typename PlayerTransferTask>
std::vector<PlayerTransferTask> getNew() {
	std::vector<PlayerTransferTask> list;
	forEachNewTask([&](const NewTask& row) {
		PlayerTransferTask task;
		task.id = row.id;
		task.sourceServerId = row.sourceServerId;
		task.targetServerId = row.targetServerId;
		task.sourceAccountId = row.sourceAccountId;
		task.targetAccountId = row.targetAccountId;
		task.playerId = row.playerId;
		list.push_back(std::move(task));
	});
	return list;
}

namespace detail {

inline std::optional<std::string_view> commentOf(const std::optional<std::string>& comment) {
	return comment ? std::optional<std::string_view>(*comment) : std::nullopt;
}

inline std::optional<std::string_view> commentOf(const std::string& comment) {
	return std::string_view(comment);
}

} // namespace detail

/**
 * Stores status and comment of the task (see update(id, status, comment)).
 *
 * @return true if the query ran successfully
 */
template <typename PlayerTransferTask>
bool update(const PlayerTransferTask& task) {
	static_assert(PlayerTransferTask::STATUS_WAIT == STATUS_WAIT && PlayerTransferTask::STATUS_ACTIVE == STATUS_ACTIVE &&
		PlayerTransferTask::STATUS_DONE == STATUS_DONE && PlayerTransferTask::STATUS_ERROR == STATUS_ERROR);
	return update(task.id, task.status, detail::commentOf(task.comment));
}

} // namespace aion::loginserver::dao::PlayerTransferDAO
