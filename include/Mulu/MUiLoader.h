#pragma once

#include <vector>

#include "Mulu/MObject.h"
#include "Mulu/MRect.h"
#include "Mulu/MSize.h"
#include "Mulu/MString.h"

namespace tinyxml2 {
class XMLElement;
}

namespace mulu {

class MWindow;
class MWidget;

// Loads Qt Designer-style .ui files (XML) and builds the equivalent MuluUI
// widget tree — the QUiLoader / uic equivalent for MuluUI.
//
// Supported markup is a pragmatic subset of the Qt Designer format:
//
//   <ui version="4.0">
//     <class>HelloForm</class>
//     <widget class="QMainWindow" name="HelloWindow">
//       <property name="geometry"><rect>
//         <x>0</x><y>0</y><width>640</width><height>480</height>
//       </rect></property>
//       <property name="windowTitle"><string>Hello</string></property>
//       <widget class="QWidget" name="centralwidget">
//         <layout class="QVBoxLayout" name="verticalLayout">
//           <item>
//             <widget class="QLabel" name="label">
//               <property name="text"><string>Hello, MuluUI!</string></property>
//             </widget>
//           </item>
//           <item>
//             <widget class="QPushButton" name="pushButton">
//               <property name="text"><string>Click me</string></property>
//             </widget>
//           </item>
//         </layout>
//       </widget>
//     </widget>
//   </ui>
//
// Class mapping: QMainWindow/QDialog/QWindow -> MWindow, QWidget -> MWidget,
// QLabel -> MLabel, QPushButton -> MButton, QVBoxLayout -> MStackLayout
// (vertical), QHBoxLayout -> MStackLayout (horizontal).
//
// Supported properties: objectName, geometry, text, visible, windowTitle,
// minimumSize, maximumSize. Unknown classes/properties are ignored with a
// warning recorded in warnings().
class MUiLoader {
public:
    MUiLoader() = default;

    // Load a .ui file and build the form.
    // Returns:
    //   - an MWindow* when the form root is a window class
    //     (QMainWindow, QDialog, QWindow, MWindow), or
    //   - an MWidget* for a plain QWidget root.
    // The returned object has no parent; ownership belongs to the caller.
    MObject* load(const MString& fileName);
    MObject* loadFromData(const MString& xmlData);

    // Load a form and install it as the central widget of `window`.
    // If the .ui root is itself a window, its content, title and size are
    // transplanted into `window`. Returns false on failure (see errorString()).
    bool loadInto(MWindow* window, const MString& fileName);

    // Diagnostics -----------------------------------------------------------
    bool hasError() const { return !m_errorString.isEmpty(); }
    MString errorString() const { return m_errorString; }
    const std::vector<MString>& warnings() const { return m_warnings; }

private:
    bool isWindowClass(const MString& className) const;
    MWidget* createWidget(const MString& className, MWidget* parent);
    MWidget* parseWidget(const tinyxml2::XMLElement* element, MWidget* parent);
    MWidget* parseLayout(const tinyxml2::XMLElement* element, MWidget* parent);
    void parseWindowProperties(MWindow* window, const tinyxml2::XMLElement* rootElement);
    void parseProperty(MWidget* widget, const tinyxml2::XMLElement* propertyElement);
    void applyText(MWidget* widget, const MString& text);
    MRect parseRect(const tinyxml2::XMLElement* rectElement) const;
    MSize parseSize(const tinyxml2::XMLElement* sizeElement) const;

    void setError(const MString& message);
    void warn(const MString& message);

    MString m_errorString;
    std::vector<MString> m_warnings;
};

} // namespace mulu
