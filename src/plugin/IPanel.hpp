#pragma once
#include <IPlugin.hpp>

namespace magic {

class Workbook;

// Plugin interface for custom ImGui panels (Phase 2).
class IPanel : public plugin_arch::IPlugin {
public:
    virtual void init(Workbook& workbook) = 0;
    virtual void render() = 0;
};

}  // namespace magic
