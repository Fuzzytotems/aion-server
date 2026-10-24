#include "aion/commons/configs/CommonsConfig.h"

#include "aion/commons/configuration/ConfigurableProcessor.h"
#include "aion/commons/configuration/transformers/JavaString.h"

namespace aion::commons::configs {

void CommonsConfig::bind(configuration::ConfigurableProcessor& processor) {
	processor.bindNamed<configuration::transformers::BasicJavaString>("RUNNABLESTATS_ENABLE", "commons.runnablestats.enable", RUNNABLESTATS_ENABLE, "false");

	// Java: SCRIPT_COMPILER_CACHING (handlers are compiled into the binary, so there is no script compiler cache). The property is still parsed
	// into a local, so that it is validated and not reported as unknown, like in Java.
	bool scriptCompilerCaching = true;
	processor.bind("commons.script_compiler.caching.enable", scriptCompilerCaching, "true");
}

} // namespace aion::commons::configs
