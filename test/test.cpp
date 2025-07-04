#include <windows.h>
#include <iostream>

int main() {
    MessageBoxA(NULL, "Hello from test program!", "MessageBox", MB_OK | MB_ICONINFORMATION);
    return 0;
}
