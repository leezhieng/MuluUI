#pragma once

#include <vector>

#include "Mulu/MString.h"

namespace mulu {

// Base class for the whole MuluUI object tree (Qt's QObject equivalent).
//
// Responsibilities:
//   - parent/child ownership: a parent deletes its children on destruction
//   - a stable object name for lookup via findChild()
//   - future home for the meta-object system (properties / signals)
class MObject {
public:
    explicit MObject(MObject* parent = nullptr);
    virtual ~MObject();

    MObject(const MObject&) = delete;
    MObject& operator=(const MObject&) = delete;

    // Parent / children -----------------------------------------------------
    MObject* parent() const { return m_parent; }
    void setParent(MObject* parent);

    const std::vector<MObject*>& children() const { return m_children; }
    MObject* findChild(const MString& name, bool recursive = true);

    // Identity --------------------------------------------------------------
    const MString& objectName() const { return m_objectName; }
    void setObjectName(const MString& name) { m_objectName = name; }

protected:
    void addChild(MObject* child);
    void removeChild(MObject* child);

private:
    MObject* m_parent = nullptr;
    std::vector<MObject*> m_children;
    MString m_objectName;
};

} // namespace mulu
