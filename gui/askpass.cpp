// ssh no acepta la contraseña por argumento: se la pide a este programa (SSH_ASKPASS).
// Imprime la variable WM_CONTRASENA, que le pasa la página de Respaldo solo a ese proceso.

#include <windows.h>

int main()
{
    wchar_t ancha[4096];
    const DWORD largo = GetEnvironmentVariableW(L"WM_CONTRASENA", ancha, 4096);
    if (largo == 0 || largo >= 4096)
        return 1;
    char texto[16384];
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, ancha, static_cast<int>(largo), texto, sizeof(texto) - 1, nullptr, nullptr);
    if (bytes <= 0)
        return 1;
    texto[bytes] = '\n';
    DWORD escritos = 0;
    return WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), texto, static_cast<DWORD>(bytes) + 1, &escritos, nullptr) ? 0 : 1;
}
