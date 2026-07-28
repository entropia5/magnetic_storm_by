#pragma once

#include <map>
#include <string>

using TranslationCatalog = std::map<
    std::string,
    std::map<std::string, std::string>
>;

extern TranslationCatalog TEXTS;
