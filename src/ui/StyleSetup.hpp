#pragma once

namespace magic {

enum class Theme { Dark, Light };

void setup_style();
void apply_theme(Theme theme);

}  // namespace magic
