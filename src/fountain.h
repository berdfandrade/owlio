#pragma once
#include <string>

#include "document.h"

// Fountain (https://fountain.io): formato aberto em texto puro para roteiros.
// É o formato nativo de salvamento do Owlio.
Document parseFountain(const std::string& utf8, bool parseTitlePage = true);
std::string writeFountain(const Document& doc, bool writeTitlePage = true);

bool looksLikeSceneHeading(const std::u32string& line);
