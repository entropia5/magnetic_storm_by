#pragma once

#include "domain_types.h"

#include <string>

std::string render_screen_image(long long chat_id, const ScreenView& view);
bool validate_screen_renderer();
