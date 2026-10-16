#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace aion::gameserver::runtime {

/**
 * Minidump writing for the watchdog (design §4.3, D5: dump and keep running). No Java counterpart (Java's DeadLockDetector only logs).
 *
 * A minidump of the live process is never written by MiniDumpWriteDump on the process itself: that call suspends every other thread while
 * DbgHelp allocates and loads modules on the dumping thread, so a suspended thread holding the loader lock or a heap lock hangs the dump and,
 * with it, the whole server (the M4 check hung inside the 45th such dump). Both ways below dump a process snapshot instead
 * (PssCaptureSnapshot with a virtual address clone): the target's threads pause only while the kernel clones the address space, and DbgHelp
 * reads the clone, so no live thread is suspended while the dump is written.
 * - Helper process (preferred): `<helper> --write-minidump <pid> <file>` takes the snapshot and writes the file; the watchdog waits at most a
 *   timeout and terminates a helper that does not finish (the target cannot be left suspended). DbgHelp then also never runs in the server,
 *   where std::stacktrace symbolization uses it concurrently (DbgHelp is single-threaded). The helper is the executable itself when its main
 *   calls runIfRequested (aion_game_server does), or an explicitly configured executable.
 * - In process (fallback for executables without the helper mode): the snapshot dump runs on a dedicated writer thread and the caller waits
 *   at most the timeout; a writer that does not finish is abandoned and the in-process way stays disabled afterwards.
 *
 * Thread-safety: all functions may be called from any thread; writes through the in-process writer thread are serialized.
 * Other platforms: every write returns UNSUPPORTED.
 */
class MinidumpWriter {
public:
	/** argv[1] of the helper mode: `--write-minidump <pid> <file>` */
	static constexpr std::string_view HELPER_ARGUMENT = "--write-minidump";

	enum class Status : uint8_t { WRITTEN, FAILED, TIMED_OUT, UNSUPPORTED };

	struct Result {
		Status status = Status::UNSUPPORTED;
		/** what happened, for the dump text ("written by helper process ...", "PssCaptureSnapshot failed: error 5", ...) */
		std::string message;

		bool written() const noexcept { return status == Status::WRITTEN; }
	};

	/**
	 * Helper mode entry point, called first in main (and by test executables at static initialization): if argv[1] is HELPER_ARGUMENT, writes a
	 * snapshot minidump of process <pid> into <file> and returns the process exit code (0 written, 1 usage error with a usage line on stderr,
	 * otherwise 0x20000000 | Win32 error); otherwise returns std::nullopt. Either way it registers the executable as a helper (registerSelfHelper).
	 */
	static std::optional<int> runIfRequested(int argc, char* argv[]);

	/** Marks the running executable as one that handles HELPER_ARGUMENT, so the watchdog may spawn it as its own helper. */
	static void registerSelfHelper() noexcept;
	static bool isSelfHelperRegistered() noexcept;
	/** path of the running executable (empty if unknown) */
	static std::filesystem::path currentExecutable();

	/** Writes a snapshot minidump (thread information and every thread's stack) of process `pid` into `file` from the calling thread. */
	static Result writeSnapshotDump(uint32_t pid, const std::filesystem::path& file);

	/** Spawns `helper --write-minidump <pid> <file>`, waits at most `timeout` and terminates the helper when it does not finish in time. */
	static Result writeWithHelper(const std::filesystem::path& helper, uint32_t pid, const std::filesystem::path& file, std::chrono::milliseconds timeout);

	/** Writes a snapshot minidump of the current process on the dedicated writer thread and waits at most `timeout` for it. */
	static Result writeInProcess(const std::filesystem::path& file, std::chrono::milliseconds timeout);

	/** Display name of a status ("WRITTEN", ...). */
	static const char* statusName(Status status) noexcept;
};

} // namespace aion::gameserver::runtime
