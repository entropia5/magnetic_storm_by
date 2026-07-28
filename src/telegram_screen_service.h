#pragma once

#include "domain_types.h"

void upsert_live_screen(
    long long chat_id,
    const ScreenView& view,
    bool force_new_message = false
);
