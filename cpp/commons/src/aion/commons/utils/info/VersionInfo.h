#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace aion::commons::utils::info {

/**
 * Java: com.aionemu.commons.utils.info.VersionInfo - the version information of a program component: its source (Java: the jar file name),
 * git revision and branch, the build date and what it was built for.
 * <p>
 * Java reads the jar manifests; the C++ build writes the same information into the generated header aion/BuildInfo.h (namespace aion::build:
 * REVISION, BRANCH, DATE), which is refreshed whenever CMake configures. Since commons and the servers are linked into one executable, commons()
 * and ofExecutable() carry the same build information and differ in their source name only.
 * <p>
 * Log output (Java: "for Java 25" becomes the C++ standard and compiler):
 * <pre>
 *          aion_commons revision 51d7a19cb (C++) built on 2026-09-12 15:42 for C++23 (MSVC 19.51.36231)
 * aion_gameserver.exe revision 51d7a19cb (C++) built on 2026-09-12 15:42 for C++23 (MSVC 19.51.36231)
 * </pre>
 *
 * @author lord_rex, Neon
 */
class VersionInfo {
public:
	/**
	 * @param source name of the component (Java: jar file name)
	 * @param revision git revision, if known
	 * @param branch git branch, if known
	 * @param buildDate build date, if known
	 * @param buildTarget what the component was built for (Java: the class file version), if known
	 */
	VersionInfo(std::string source, std::optional<std::string> revision, std::optional<std::string> branch,
		std::optional<std::chrono::sys_seconds> buildDate, std::optional<std::string> buildTarget);

	/** Java: VersionInfo.commons - the version information of the commons library (source "aion_commons") */
	static const VersionInfo& commons();

	/**
	 * Java: new VersionInfo(GameServer.class) - the version information of the running executable (source: its file name), with the build
	 * information of aion/BuildInfo.h.
	 */
	static VersionInfo ofExecutable();

	/**
	 * Creates version information from the values of aion/BuildInfo.h: "unknown" or empty revisions and branches are treated as absent, the date
	 * ("yyyy-MM-ddTHH:mm:ss", optionally followed by Z for UTC) is interpreted in the system default time zone if it has no Z.
	 */
	static VersionInfo fromBuildInfo(std::string source, std::string_view revision, std::string_view branch, std::string_view date);

	const std::string& getSource() const noexcept { return source; }
	const std::optional<std::string>& getRevision() const noexcept { return revision; }
	const std::optional<std::string>& getBranch() const noexcept { return branch; }
	/** Java: getBuildDate() */
	const std::optional<std::chrono::sys_seconds>& getBuildDate() const noexcept { return buildDate; }
	/** Java: getClassFileVersion() - e.g. "C++23 (MSVC 19.51.36231)" */
	const std::optional<std::string>& getBuildTarget() const noexcept { return buildTarget; }

	/**
	 * Java: getBuildInfo(ZoneId), e.g. "revision 51d7a19cb (C++) built on 2026-09-12 15:42 for C++23 (MSVC 19.51.36231)".
	 *
	 * @param timeZone time zone of the build date, nullptr for the system default time zone
	 */
	std::string getBuildInfo(const std::chrono::time_zone* timeZone) const;

	/** Java: toString() - source and build info in the system default time zone */
	std::string toString() const;

	/** Java: toString(ZoneId) */
	std::string toString(const std::chrono::time_zone* timeZone) const;

	/** Java: logAll(Class) - logs the version information of commons and the running executable. */
	static void logAll();

	/** Java: logAll(VersionInfo, ZoneId) - logs the version information of commons and the given component, with right aligned sources. */
	static void logAll(const VersionInfo& versionInfo, const std::chrono::time_zone* timeZone);

private:
	std::string toString(const std::chrono::time_zone* timeZone, size_t sourceLeftPadToWidth) const;

	std::string source;
	std::optional<std::string> revision;
	std::optional<std::string> branch;
	std::optional<std::chrono::sys_seconds> buildDate;
	std::optional<std::string> buildTarget;
};

} // namespace aion::commons::utils::info
