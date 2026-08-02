// Hello MuluUI - demonstrates the core M-widget API running on WinUI3.
#include <Mulu/Mulu.h>

using namespace mulu;

int main(int argc, char** argv)
{
    MApplication app(argc, argv);
    app.setApplicationName("MuluHello");

    MWindow window;
    window.setTitle("Hello MuluUI");
    window.setSize(MSize(640, 480));

    auto* root = new MStackLayout();
    auto* label = new MLabel("Hello, MuluUI!");
    auto* button = new MButton("Click me");

    root->addWidget(label);
    root->addWidget(button);

    button->setOnClicked([label]() {
        label->setText("Button clicked!");
    });

    window.setCentralWidget(root);
    window.show();

    return app.exec();
}
