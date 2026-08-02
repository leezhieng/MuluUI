// Loads a Qt Designer-style .ui file and builds the UI with MuluUI.
#include <iostream>

#include <Mulu/Mulu.h>
#include <Mulu/MUiLoader.h>

using namespace mulu;

int main(int argc, char** argv)
{
    MApplication app(argc, argv);
    app.setApplicationName("MuluUiLoader");

    MUiLoader loader;
    MObject* form = loader.load(MULU_FORM_UI);
    if (!form) {
        std::cerr << "Failed to load UI: " << loader.errorString() << "\n";
        return 1;
    }

    for (const MString& warning : loader.warnings()) {
        std::cerr << "UI warning: " << warning << "\n";
    }

    MWindow* window = dynamic_cast<MWindow*>(form);
    if (!window) {
        // The form root was a plain widget; wrap it in a window.
        window = new MWindow();
        window->setCentralWidget(static_cast<MWidget*>(form));
    }

    // Connect widgets by object name using the object tree built from the .ui.
    auto* button = dynamic_cast<MButton*>(window->findChild("pushButton"));
    auto* label = dynamic_cast<MLabel*>(window->findChild("label"));
    auto* closeButton = dynamic_cast<MButton*>(window->findChild("closeButton"));

    if (button && label) {
        button->setOnClicked([label]() { label->setText("Button clicked!"); });
    }
    if (closeButton) {
        closeButton->setOnClicked([window]() { window->close(); });
    }

    window->show();
    return app.exec();
}
