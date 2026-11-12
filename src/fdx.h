#pragma once
#include <string>

#include "document.h"

// Final Draft XML (.fdx) — permite trocar arquivos com o Final Draft.
std::string writeFdx(const Document& doc);
Document parseFdx(const std::string& xml);
