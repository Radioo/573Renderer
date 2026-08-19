#include "preset/defaults/defaults.h"

#include "preset/defaults/defaults_build.h"
#include "preset/doc/preset_document.h"

#include <utility>
#include <vector>

namespace Preset::Doc {

std::vector<Document> BuiltIns() {
    std::vector<Document> documents = Iidx10Defaults();
    for (std::vector<Document> group : {Iidx11Defaults(), Iidx11SelectDefaults()}) {
        for (Document& document : group)
            documents.push_back(std::move(document));
    }
    documents.push_back(Iidx11Ending());
    for (Document& document : Iidx12Defaults())
        documents.push_back(std::move(document));
    return documents;
}

}
