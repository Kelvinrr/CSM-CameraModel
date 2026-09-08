// Reproduces how ISIS/ASP load the plugin: links csmapi, does NOT link libusgscsm,
// dlopens it. Fails if the plugin registers into a private csm::Plugin::theList
// instead of this process's registry, which is what happens when csmapi is linked
// into the plugin itself.

#include <csm/Plugin.h>

#include <dlfcn.h>

#include <iostream>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <path to libusgscsm>\n";
    return 2;
  }

  if (!csm::Plugin::getList().empty()) {
    std::cerr << "registry unexpectedly non-empty before dlopen\n";
    return 1;
  }

  if (dlopen(argv[1], RTLD_LAZY) == nullptr) {
    std::cerr << "dlopen failed: " << dlerror() << "\n";
    return 1;
  }

  const csm::PluginList &plugins = csm::Plugin::getList();
  for (const csm::Plugin *plugin : plugins) {
    if (plugin->getPluginName() == "UsgsAstroPluginCSM") {
      return 0;
    }
  }

  std::cerr << "UsgsAstroPluginCSM did not register with the host; the host sees "
            << plugins.size() << " plugin(s)\n";
  return 1;
}
