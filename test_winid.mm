#import <AppKit/AppKit.h>
#import <QtWidgets/QApplication>
#import <QtWidgets/QWidget>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QWidget w;
    w.resize(640, 480);
    w.show();
    
    WId winId = w.winId();
    NSView* view = (NSView*)winId;
    NSWindow* window = [view window];
    
    NSLog(@"winId: %p", winId);
    NSLog(@"view: %@", view);
    NSLog(@"window: %@", window);
    
    return app.exec();
}
