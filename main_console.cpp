#include <QTextStream>  
static QTextStream cout(stdout, QIODevice::WriteOnly);

int main(int argc, char* argv[]) {
    cout << "Hello" << endl;
    return 0;
}
