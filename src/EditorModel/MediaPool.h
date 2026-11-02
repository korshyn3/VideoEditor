// MediaPool.h
// Capa: MODEL.
//
// Conjunto de archivos fuente importados en el proyecto. Los clips los referencian por Id.
#pragma once

#include "EditorModel/Ids.h"
#include "EditorModel/MediaInfo.h"

#include <map>

namespace model {

class MediaPool {
public:
    void add(Id id, MediaInfo info) { items_[id] = std::move(info); }

    const MediaInfo* find(Id id) const {
        auto it = items_.find(id);
        return it != items_.end() ? &it->second : nullptr;
    }

    const std::map<Id, MediaInfo>& items() const { return items_; }
    void setOffline(Id id, bool offline) {
        if (auto it = items_.find(id); it != items_.end()) it->second.offline = offline;
    }

private:
    std::map<Id, MediaInfo> items_;
};

} // namespace model
