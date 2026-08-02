#include "Mulu/MUiLoader.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <tinyxml2.h>

#include "Mulu/MButton.h"
#include "Mulu/MLabel.h"
#include "Mulu/MStackLayout.h"
#include "Mulu/MWidget.h"
#include "Mulu/MWindow.h"

namespace mulu {

namespace {

// Read an entire file into a std::string. UTF-8 aware on Windows so that
// non-ASCII .ui paths work.
bool readFile(const MString& fileName, std::string& out)
{
    FILE* file = nullptr;
#ifdef _WIN32
    if (_wfopen_s(&file, fileName.toStdWString().c_str(), L"rb") != 0) {
        return false;
    }
#else
    file = std::fopen(fileName.c_str(), "rb");
    if (!file) {
        return false;
    }
#endif

    bool ok = false;
    if (std::fseek(file, 0, SEEK_END) == 0) {
        const long length = std::ftell(file);
        if (length >= 0 && std::fseek(file, 0, SEEK_SET) == 0) {
            out.resize(static_cast<size_t>(length));
            ok = std::fread(&out[0], 1, static_cast<size_t>(length), file) ==
                 static_cast<size_t>(length);
        }
    }
    std::fclose(file);
    return ok;
}

// Read an integer child element (e.g. <width>123</width>) with a fallback.
int intOf(const tinyxml2::XMLElement* parent, const char* childName, int fallback = 0)
{
    if (const tinyxml2::XMLElement* child = parent->FirstChildElement(childName)) {
        return child->IntText(fallback);
    }
    return fallback;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

MObject* MUiLoader::load(const MString& fileName)
{
    std::string content;
    if (!readFile(fileName, content)) {
        MString message("cannot open file: ");
        message.append(fileName);
        setError(message);
        return nullptr;
    }
    return loadFromData(MString(std::move(content)));
}

MObject* MUiLoader::loadFromData(const MString& xmlData)
{
    m_errorString.clear();
    m_warnings.clear();

    tinyxml2::XMLDocument document;
    if (document.Parse(xmlData.c_str(), static_cast<size_t>(xmlData.size())) !=
        tinyxml2::XML_SUCCESS) {
        MString message("XML parse error at line ");
        message.append(std::to_string(document.ErrorLineNum()));
        setError(message);
        return nullptr;
    }

    const tinyxml2::XMLElement* ui = document.FirstChildElement("ui");
    if (!ui) {
        setError("missing <ui> root element");
        return nullptr;
    }

    const tinyxml2::XMLElement* rootElement = ui->FirstChildElement("widget");
    if (!rootElement) {
        setError("missing <widget> element");
        return nullptr;
    }

    const char* className = rootElement->Attribute("class");
    const char* objectName = rootElement->Attribute("name");
    const MString rootClass = className ? className : "QWidget";

    // Window-type root: build an MWindow around the form.
    if (isWindowClass(rootClass)) {
        auto* window = new MWindow();
        if (objectName) {
            window->setObjectName(objectName);
        }
        parseWindowProperties(window, rootElement);

        // The first child <widget> of a window root is its central widget
        // (Qt Designer emits it as e.g. name="centralwidget").
        bool centralInstalled = false;
        for (const tinyxml2::XMLElement* child =
                 rootElement->FirstChildElement("widget");
             child; child = child->NextSiblingElement("widget")) {
            if (MWidget* central = parseWidget(child, nullptr)) {
                window->setCentralWidget(central);
                centralInstalled = true;
                break;
            }
        }

        // Some forms place a layout directly on the root widget.
        if (!centralInstalled) {
            for (const tinyxml2::XMLElement* layoutElement =
                     rootElement->FirstChildElement("layout");
                 layoutElement;
                 layoutElement = layoutElement->NextSiblingElement("layout")) {
                if (MWidget* layout = parseLayout(layoutElement, nullptr)) {
                    window->setCentralWidget(layout);
                    centralInstalled = true;
                    break;
                }
            }
        }

        return window;
    }

    // Plain widget root.
    return parseWidget(rootElement, nullptr);
}

bool MUiLoader::loadInto(MWindow* window, const MString& fileName)
{
    if (!window) {
        setError("loadInto: window is null");
        return false;
    }

    MObject* form = load(fileName);
    if (!form) {
        return false;
    }

    if (auto* formWindow = dynamic_cast<MWindow*>(form)) {
        // The .ui root is a window: transplant its content into `window`.
        if (MWidget* central = formWindow->centralWidget()) {
            central->setParent(window); // detach before deleting formWindow
            window->setCentralWidget(central);
        }
        window->setTitle(formWindow->title());
        if (!formWindow->size().isEmpty()) {
            window->setSize(formWindow->size());
        }
        delete formWindow;
        return true;
    }

    if (auto* widget = dynamic_cast<MWidget*>(form)) {
        window->setCentralWidget(widget);
        return true;
    }

    delete form;
    setError("loadInto: unsupported form root");
    return false;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

bool MUiLoader::isWindowClass(const MString& className) const
{
    return className == "QMainWindow" || className == "QDialog" ||
           className == "QWindow" || className == "MWindow";
}

MWidget* MUiLoader::createWidget(const MString& className, MWidget* parent)
{
    if (className == "QLabel") {
        return new MLabel(parent);
    }
    if (className == "QPushButton" || className == "QButton" ||
        className == "MButton") {
        return new MButton(parent);
    }
    if (className == "MStackLayout") {
        return new MStackLayout(parent);
    }
    if (className != "QWidget") {
        MString message("unknown widget class '");
        message.append(className).append("'; using a generic MWidget");
        warn(message);
    }
    return new MWidget(parent);
}

MWidget* MUiLoader::parseWidget(const tinyxml2::XMLElement* element, MWidget* parent)
{
    const char* className = element->Attribute("class");
    const char* objectName = element->Attribute("name");

    MWidget* widget = createWidget(className ? className : "QWidget", parent);
    if (objectName) {
        widget->setObjectName(objectName);
    }

    // Properties.
    for (const tinyxml2::XMLElement* property =
             element->FirstChildElement("property");
         property; property = property->NextSiblingElement("property")) {
        parseProperty(widget, property);
    }

    // Layouts declared on this widget.
    for (const tinyxml2::XMLElement* layoutElement =
             element->FirstChildElement("layout");
         layoutElement;
         layoutElement = layoutElement->NextSiblingElement("layout")) {
        parseLayout(layoutElement, widget);
    }

    // Plain child widgets (absolute-positioned containers without layouts).
    for (const tinyxml2::XMLElement* childElement =
             element->FirstChildElement("widget");
         childElement;
         childElement = childElement->NextSiblingElement("widget")) {
        parseWidget(childElement, widget);
    }

    return widget;
}

MWidget* MUiLoader::parseLayout(const tinyxml2::XMLElement* element, MWidget* parent)
{
    const char* className = element->Attribute("class");
    const char* objectName = element->Attribute("name");

    auto* layout = new MStackLayout(parent);
    if (objectName) {
        layout->setObjectName(objectName);
    }

    const MString layoutClass = className ? className : "QVBoxLayout";
    if (layoutClass == "QHBoxLayout") {
        layout->setOrientation(MStackLayout::Orientation::Horizontal);
    } else if (layoutClass == "QGridLayout") {
        warn("QGridLayout is not supported yet; using a vertical stack");
    } else if (layoutClass != "QVBoxLayout" && layoutClass != "MStackLayout") {
        MString message("unknown layout class '");
        message.append(layoutClass).append("'; using a vertical stack");
        warn(message);
    }

    for (const tinyxml2::XMLElement* item = element->FirstChildElement("item");
         item; item = item->NextSiblingElement("item")) {
        if (const tinyxml2::XMLElement* childWidget =
                item->FirstChildElement("widget")) {
            if (MWidget* child = parseWidget(childWidget, layout)) {
                layout->addWidget(child);
            }
        } else if (const tinyxml2::XMLElement* childLayout =
                       item->FirstChildElement("layout")) {
            if (MWidget* child = parseLayout(childLayout, layout)) {
                layout->addWidget(child);
            }
        }
    }

    return layout;
}

void MUiLoader::parseWindowProperties(MWindow* window,
                                      const tinyxml2::XMLElement* rootElement)
{
    for (const tinyxml2::XMLElement* property =
             rootElement->FirstChildElement("property");
         property; property = property->NextSiblingElement("property")) {
        const char* name = property->Attribute("name");
        if (!name) {
            continue;
        }
        const MString propertyName(name);

        if (propertyName == "windowTitle") {
            if (const tinyxml2::XMLElement* value =
                    property->FirstChildElement("string")) {
                window->setTitle(value->GetText() ? value->GetText() : "");
            }
        } else if (propertyName == "geometry") {
            if (const tinyxml2::XMLElement* rect =
                    property->FirstChildElement("rect")) {
                const MSize size = parseSize(rect);
                if (!size.isEmpty()) {
                    window->setSize(size);
                }
            }
        } else if (propertyName == "minimumSize") {
            if (const tinyxml2::XMLElement* size =
                    property->FirstChildElement("size")) {
                window->setMinimumSize(parseSize(size));
            }
        } else if (propertyName == "maximumSize") {
            if (const tinyxml2::XMLElement* size =
                    property->FirstChildElement("size")) {
                window->setMaximumSize(parseSize(size));
            }
        }
        // Other window properties are intentionally ignored.
    }
}

void MUiLoader::parseProperty(MWidget* widget,
                              const tinyxml2::XMLElement* propertyElement)
{
    const char* name = propertyElement->Attribute("name");
    if (!name) {
        return;
    }
    const MString propertyName(name);

    if (propertyName == "objectName") {
        if (const tinyxml2::XMLElement* value =
                propertyElement->FirstChildElement("string")) {
            if (value->GetText()) {
                widget->setObjectName(value->GetText());
            }
        }
    } else if (propertyName == "geometry") {
        if (const tinyxml2::XMLElement* rect =
                propertyElement->FirstChildElement("rect")) {
            widget->setGeometry(parseRect(rect));
        }
    } else if (propertyName == "text") {
        if (const tinyxml2::XMLElement* value =
                propertyElement->FirstChildElement("string")) {
            applyText(widget, value->GetText() ? value->GetText() : "");
        }
    } else if (propertyName == "visible") {
        if (const tinyxml2::XMLElement* value =
                propertyElement->FirstChildElement("bool")) {
            const char* text = value->GetText();
            widget->setVisible(text && std::strcmp(text, "true") == 0);
        }
    }
    // Other properties (font, palette, styleSheet, ...) are not mapped yet.
}

void MUiLoader::applyText(MWidget* widget, const MString& text)
{
    if (auto* label = dynamic_cast<MLabel*>(widget)) {
        label->setText(text);
    } else if (auto* button = dynamic_cast<MButton*>(widget)) {
        button->setText(text);
    } else {
        MString message("'text' property ignored on non-text widget '");
        message.append(widget->objectName()).append("'");
        warn(message);
    }
}

MRect MUiLoader::parseRect(const tinyxml2::XMLElement* rectElement) const
{
    return MRect(intOf(rectElement, "x"),
                 intOf(rectElement, "y"),
                 intOf(rectElement, "width"),
                 intOf(rectElement, "height"));
}

MSize MUiLoader::parseSize(const tinyxml2::XMLElement* sizeElement) const
{
    return MSize(intOf(sizeElement, "width"), intOf(sizeElement, "height"));
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

void MUiLoader::setError(const MString& message)
{
    m_errorString = message;
}

void MUiLoader::warn(const MString& message)
{
    m_warnings.push_back(message);
}

} // namespace mulu
