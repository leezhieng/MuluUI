// MuluUI SDL3 + OpenGL 3.3 backend — demo example.
#include <Mulu/Mulu.h>

#include <cstdio>

#ifdef MULU_HAS_EMBEDDED_FONT
extern const unsigned char kFontData[];
extern const unsigned int  kFontDataSize;
#endif

int main(int argc, char** argv)
{
    using namespace mulu;

    std::printf("MuluUI SDL3 + OpenGL 3.3 Example\n");
    std::printf("================================\n\n");

    MApplication app(argc, argv);
    app.setApplicationName("MuluSDLHello");

    MWindow window;
    window.setTitle("MuluUI — SDL3 + OpenGL 3.3");
    window.setSize(MSize(640, 480));

#ifdef MULU_HAS_EMBEDDED_FONT
    window.setFontData(kFontData, kFontDataSize);
#endif

    auto* rootLayout = new MStackLayout();
    rootLayout->setOrientation(MStackLayout::Orientation::Vertical);

    auto* label = new MLabel("Hello from MuluUI + SDL3 + OpenGL!");
    label->setGeometry(MRect(20, 20, 400, 30));
    rootLayout->addWidget(label);

    auto* button = new MButton("Click Me");
    button->setGeometry(MRect(20, 60, 160, 40));
    rootLayout->addWidget(button);

    auto* statusLabel = new MLabel("Status: waiting for click...");
    statusLabel->setGeometry(MRect(20, 110, 400, 30));
    rootLayout->addWidget(statusLabel);

    button->setOnClicked([statusLabel]() {
        statusLabel->setText("Status: button was clicked!");
        std::printf("Button clicked — status updated\n");
    });

    window.setCentralWidget(rootLayout);
    window.show();

    std::printf("Entering event loop...\n");
    const int result = app.exec();
    std::printf("Event loop exited with code %d\n", result);

    return result;
}
