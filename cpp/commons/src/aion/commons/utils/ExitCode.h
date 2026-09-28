#pragma once

#include <cstdint>

/**
 * Java: com.aionemu.commons.utils.ExitCode - exit codes of the servers. The start scripts restart the server on RESTART and report an abnormal
 * termination on any code other than NORMAL.
 *
 * @author SoulKeeper
 */
namespace aion::commons::utils::ExitCode {

/** Indicates that server successfully finished it's work */
inline constexpr int32_t NORMAL = 0;

/** Indicates that server successfully finished it's work and should be restarted */
inline constexpr int32_t RESTART = 2;

/**
 * Indicates that error happened in server and it's need to shutdown.
 * <p>
 * Java: ERROR. Renamed because &lt;wingdi.h&gt; (pulled in by &lt;windows.h&gt;, e.g. via Asio) defines an ERROR macro.
 */
inline constexpr int32_t ERROR_ = 1;

} // namespace aion::commons::utils::ExitCode
