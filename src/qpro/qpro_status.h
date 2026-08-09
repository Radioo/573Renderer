#pragma once

#include "qpro/qpro_extract.h"

#include <string>

namespace QproExtract {

void PublishProgress(const char* stage);

void Note(Result& res, bool failure, const std::string& label, const std::string& ifs,
          const std::string& reason);

void NoteIssue(const std::string& text, bool failure);

void BumpDone();

int DoneCount();

int TotalCount();

}
