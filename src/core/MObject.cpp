#include "Mulu/MObject.h"

#include <algorithm>

namespace mulu {

MObject::MObject(MObject* parent)
    : m_parent(parent)
{
    if (parent) {
        parent->addChild(this);
    }
}

MObject::~MObject()
{
    // Delete children first: ownership flows from parent to child.
    for (MObject* child : m_children) {
        delete child;
    }
    m_children.clear();

    if (m_parent) {
        m_parent->removeChild(this);
    }
    m_parent = nullptr;
}

void MObject::setParent(MObject* parent)
{
    if (m_parent == parent) {
        return;
    }
    if (m_parent) {
        m_parent->removeChild(this);
    }
    m_parent = parent;
    if (m_parent) {
        m_parent->addChild(this);
    }
}

void MObject::addChild(MObject* child)
{
    m_children.push_back(child);
}

void MObject::removeChild(MObject* child)
{
    const auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

MObject* MObject::findChild(const MString& name, bool recursive)
{
    for (MObject* child : m_children) {
        if (child->objectName() == name) {
            return child;
        }
    }
    if (recursive) {
        for (MObject* child : m_children) {
            if (MObject* found = child->findChild(name, true)) {
                return found;
            }
        }
    }
    return nullptr;
}

} // namespace mulu
