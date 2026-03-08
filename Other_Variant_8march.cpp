//2D версия сердечка

#include <windows.h>
#include <iostream>

int main() {
    int size = 1;
    std::cout << "Enter size of heart: ";
    std::cin >> size;

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return 1;

    // Сохраняем текущие атрибуты, чтобы потом вернуть
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    WORD oldAttrs = 0;
    if (GetConsoleScreenBufferInfo(hOut, &csbi)) oldAttrs = csbi.wAttributes;

    // Установим ярко-красный (красный + интенсивность)
    SetConsoleTextAttribute(hOut, FOREGROUND_RED | FOREGROUND_INTENSITY);
    std::cout << "Red text\n";

    // Верхняя часть (две дуги)
    for (int i = size / 2; i <= size; i += 2) {
        for (int j = 1; j < size - i; j += 2) std::cout << " ";
        for (int j = 1; j <= i; j++) std::cout << "@";
        for (int j = 1; j <= size - i; j++) std::cout << " ";
        for (int j = 1; j <= i; j++) std::cout << "@";
        std::cout << std::endl;
    }

    // Нижняя часть (треугольник)
    for (int i = size; i >= 1; i--) {
        for (int j = i; j < size; j++) std::cout << " ";
        for (int j = 1; j <= (i * 2) - 1; j++) std::cout << "@";
        std::cout << std::endl;
    }



    // Возвращаем прежний цвет
    SetConsoleTextAttribute(hOut, oldAttrs);
    std::cout << "End of script\n";

    return 0;
}
