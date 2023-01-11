#include <fplus/fplus.hpp>
#include <emscripten.h>
#include "HyperlinkHelper.h"

namespace HyperlinkHelper {
    void OpenUrl(const std::string &url) {
        bool isAbsoluteUrl = fplus::is_prefix_of(std::string("http"), url);
        if (!isAbsoluteUrl) {
            return;
        }
        std::string js_command = "window.open(\"" + url + "\");";
        emscripten_run_script(js_command.c_str());
    }
}

