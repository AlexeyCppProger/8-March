// Версия без движения, можно почитать подробно как всё устроено

#define NOMINMAX
#include <Windows.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <iostream>

/*
    Общее описание:
    Эта программа рендерит поверхность "сердца" (Heart Surface) в буфер пикселей
    и выводит результат в клиентскую область консольного окна Windows с помощью
    функции StretchDIBits. Основная идея — для каждого пикселя вычислить
    координаты фрагмента в пространстве, определить, попадает ли точка в объём
    сердца (с помощью функции f), вычислить нормаль и освещение, затем записать
    цвет в буфер и вывести как изображение.

    Структура кода:
    - Простейшие векторы vec2 и vec3 и операции над ними.
    - Функция f — скалярная функция, задающая поверхность сердца в трёхмерном пространстве.
    - Функция h — вычисляет значение y для заданных x, z методом бисекции.
    - Функция normal — вычисляет нормаль поверхности в точке.
    - Функция shade — вычисляет цвет пикселя (освещение, материалы).
    - В main: получение дескриптора окна/контекста устройства, подготовка буфера,
      рендеринг пикселей, вывод через StretchDIBits.
*/

/* -------------------- Векторная арифметика -------------------- */

/*
    vec2 — двухмерный вектор, используется для координат экранных фрагментов.
    Конструктор задаёт компоненты x и y.
*/
struct vec2
{
    float x, y;
    vec2() : x(0), y(0) {}
    vec2(float a, float b) : x(a), y(b) {}
};

/* Умножение vec2 на скаляр (правый операнд) */
vec2 operator*(vec2 const &lh, float rh) { return vec2(lh.x * rh, lh.y * rh); }

/* Вычитание двух vec2 */
vec2 operator-(vec2 const &lh, vec2 const &rh) { return vec2(lh.x - rh.x, lh.y - rh.y); }

/*
    vec3 — трёхмерный вектор, используется для точек/нормалей/цветов.
    Конструкторы и операции ниже — простые элементные операции.
*/
struct vec3
{
    float x, y, z;
    vec3() : x(0), y(0), z(0) {}
    vec3(float a, float b, float c) : x(a), y(b), z(c) {}
};

vec3 operator+(vec3 const &lh, vec3 const &rh) { return vec3(lh.x + rh.x, lh.y + rh.y, lh.z + rh.z); }
vec3 operator*(vec3 const &lh, vec3 const &rh) { return vec3(lh.x * rh.x, lh.y * rh.y, lh.z * rh.z); }
vec3 operator*(vec3 const &lh, float rh) { return vec3(lh.x * rh, lh.y * rh, lh.z * rh); }

/* Скалярное (dot) произведение двух vec3 */
float dot(vec3 const &lh, vec3 const &rh) { return lh.x * rh.x + lh.y * rh.y + lh.z * rh.z; }

/* Нормализация вектора: возвращает v / |v|; защита от деления на ноль */
vec3 normalize(vec3 const &v)
{
    float d = std::sqrt(dot(v, v));
    if (d == 0.0f) return vec3(0.f, 1.f, 0.f); // безопасный дефолт для нулевого вектора
    return v * (1.0f / d);
}

/* -------------------- Математическая модель сердца -------------------- */

/*
    Функция f(p) описывает "heart surface" — область в трёхмерном пространстве.
    Если f(p) <= 0, то точка p принадлежит объёму/поверхности сердца.
    Формула взята из источников (пример в Internet по HeartSurface).
    Здесь используется поэлементное возведение в степень через оператор *.
*/
float f(vec3 p)
{
    vec3 pp = p * p;     // квадрат поэлементно (x^2, y^2, z^2)
    vec3 ppp = pp * p;   // куб поэлементно (x^3, y^3, z^3)
    float a = pp.x + 2.25f * pp.y + pp.z - 1.0f;
    return a * a * a - (pp.x + 0.1125f * pp.y) * ppp.z;
}

/*
    Функция h(x, z) — решатель по y: для фиксированных x и z находит такое y,
    при котором точка (x, y, z) лежит на границе сердца. Используется простой
    метод бисекции на отрезке [0, 0.75] с фиксированным числом итераций.
    Возвращает приближённое значение y.
*/
float h(float x, float z)
{
    float a = 0.0f, b = 0.75f, y = 0.5f;
    for (int i = 0; i < 10; i++)
    {
        if (f(vec3(x, y, z)) <= 0.0f)
            a = y;
        else
            b = y;
        y = (a + b) * 0.5f;
    }
    return y;
}

/*
    Функция normal(p) — вычисляет нормаль поверхности в точке p = (x,?,z),
    где середина по y берётся через h(x, z). Вычисление идёт аналитически
    из уравнения поверхности (градиент), здесь представлено в виде
    заранее выведенного выражения. После вычисления вектор нормализуется.
*/
vec3 normal(vec2 p)
{
    // v = (x, y_on_surface, z)
    vec3 v = vec3(p.x, h(p.x, p.y), p.y);
    vec3 vv = v * v;
    vec3 vvv = vv * v;
    float a = -1.0f + dot(vv, vec3(1.f, 2.25f, 1.f));
    a *= a;

    return normalize(vec3(
        -2.0f * v.x * vvv.z + 6.0f * v.x * a,
        -0.225f * v.y * vvv.z + 13.5f * v.y * a,
        v.z * (-3.0f * vv.x * v.z - 0.3375f * vv.y * v.z + 6.0f * a)));
}

/* -------------------- Освещение и шейдинг -------------------- */

/*
    shade(fragCoord, resolution) — основная функция шейдинга для одного пикселя.
    - fragCoord: координаты пикселя в экранных пикселях (x, y), где (0,0) — верхний левый.
    - resolution: размер буфера в пикселях (width, height).

    Порядок действий:
    1) Преобразуем экранные координаты в нормализованные координаты t в диапазоне,
       примерно, от -1 до 1, с учётом соотношения сторон.
    2) Строим трёхмерную точку p и масштабируем её (tp).
    3) Проверяем, лежит ли соответствующая точка в объёме сердца (f <= 0).
       Если да — вычисляем нормаль и компоненты освещения (diffuse, specular, rim),
       комбинируем их для получения цвета поверхности (красный).
       Если нет — даём фон/небо (тон кожи/фон) с падением яркости по расстоянию.
*/
vec3 shade(vec2 fragCoord, vec2 resolution)
{
    // Преобразование координат: сдвиг в центр и нормализация по меньшей стороне
    vec2 t = (fragCoord * 2.0f - resolution) * (1.f / std::min(resolution.y, resolution.x));
    vec3 p = vec3(t.x, t.y, 0.f);

    // Местный масштаб для "объёма"
    vec3 tp = p * vec3(2.0f, 2.0f, 0.0f);

    // Проверяем принадлежность в объёму сердца; важный нюанс — порядок координат, т.к.
    // в определении f ожидается vec3(x, y, z) по своей логике. Здесь используется
    // vec3(tp.x, tp.z, tp.y) — это трансформация, взятая из исходного примера.
    if (f(vec3(tp.x, tp.z, tp.y)) <= 0.0f)
    {
        // внутри/на поверхности — вычисляем нормаль, освещение и итоговый цвет
        vec3 n = normal(vec2(tp.x, tp.y));
        float diffuse = dot(n, normalize(vec3(-1.f, 1.f, 1.f))) * 0.5f + 0.5f;
        float specular = std::pow(std::max(dot(n, normalize(vec3(-1.f, 2.f, 1.f))), 0.0f), 64.f);
        float rim = 1.0f - dot(n, vec3(0.f, 1.f, 0.f));
        // комбинируем красный основной цвет + серые блики + rim-light
        return vec3(1.f, 0.f, 0.f) * diffuse + vec3(0.8f, 0.8f, 0.8f) * specular + vec3(0.5f, 0.5f, 0.5f) * rim;
    }
    else
        // фон: лёгкая вариация по y и затемнение по расстоянию от центра
        return vec3(1.f, 0.8f, 0.7f - 0.07f * p.y) * (1.f - 0.15f * std::sqrt(dot(p, p)));
}

/* -------------------- Утилиты для цвета/округления -------------------- */

/* clampf — ограничение значения между lo и hi */
float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/*
    my_lround — простая реализация округления float -> int.
    (std::lround есть в стандартной библиотеке, но даём простую альтернативу.)
*/
int my_lround(float f)
{
    int i = static_cast<int>(f);
    return (f - i < 0.5f) ? i : (i + 1);
}

/*
    toBGRA — преобразует вектор цвета (r,g,b) с компонентами в [0,1]
    в 32-битный формат BGRA, где старший байт — alpha (0xFF).
    Заметим: оформление в формате 0xAARRGGBB, но функция именована toBGRA
    чтобы соответствовать ожидаемому порядку байт при DIB (зависит от платформы).
*/
std::uint32_t toBGRA(vec3 color)
{
    const std::uint32_t r = 0xff & static_cast<std::uint32_t>(my_lround(clampf(color.x, 0.0f, 1.f) * 255.f));
    const std::uint32_t g = 0xff & static_cast<std::uint32_t>(my_lround(clampf(color.y, 0.0f, 1.f) * 255.f));
    const std::uint32_t b = 0xff & static_cast<std::uint32_t>(my_lround(clampf(color.z, 0.0f, 1.f) * 255.f));
    return (0xFF000000u) | (r << 16) | (g << 8) | (b);
}

/* -------------------- main: Windows-специфичный код вывода -------------------- */

int main()
{
    // Получаем HWND текущего консольного окна.
    // Если программа запущена в окружении без консоли — GetConsoleWindow может вернуть NULL.
    HWND win = ::GetConsoleWindow();
    if (!win)
    {
        // Сообщения в консоль — оставляем на английском по просьбе.
        std::cerr << "GetConsoleWindow() returned NULL\n";
        return 1;
    }

    // Инициализируем структуру WINDOWINFO и обязательно задаём cbSize перед вызовом GetWindowInfo.
    // Если этого не сделать, GetWindowInfo может вернуть некорректные данные.
    WINDOWINFO wi = {};
    wi.cbSize = sizeof(WINDOWINFO);
    if (!::GetWindowInfo(win, &wi))
    {
        std::cerr << "GetWindowInfo failed\n";
        return 1;
    }

    // Получаем контекст устройства (Device Context) для окна консоли.
    HDC dc = ::GetDC(win);
    if (!dc)
    {
        std::cerr << "GetDC failed\n";
        return 1;
    }

    // Размер клиентской области консоли (ширина и высота).
    int w = wi.rcClient.right - wi.rcClient.left;
    int h = wi.rcClient.bottom - wi.rcClient.top;
    if (w <= 0 || h <= 0)
    {
        std::cerr << "Invalid client size: " << w << "x" << h << "\n";
        ::ReleaseDC(win, dc);
        return 1;
    }

    // Резервируем вектор пикселей (каждый пиксель — 32-битное значение).
    std::vector<std::uint32_t> pixels;
    try {
        pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    } catch (...) {
        std::cerr << "Failed to allocate pixel buffer\n";
        ::ReleaseDC(win, dc);
        return 1;
    }

    /*
        Заполняем буфер пикселей.
        ВАЖНЫЙ МОМЕНТ про ориентировку (flip):
        - Мы используем DIB с отрицательной высотой (см. ниже: bmi.bmiHeader.biHeight = -h),
          то есть буфер рассматривается как top-down: pixels[0] соответствует верхней строке.
        - Поэтому здесь обращаемся в shade с координатой y в обычном порядке (0..h-1),
          где 0 — верхняя строка. Это обеспечивает корректную ориентацию изображения.
    */
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            pixels[y * w + x] = toBGRA(
                shade(vec2(static_cast<float>(x), static_cast<float>(y)),
                      vec2(static_cast<float>(w), static_cast<float>(h)))
            );

    /*
        Подготовка BITMAPINFO для StretchDIBits.
        - biSize: размер заголовка.
        - biWidth: ширина в пикселях.
        - biHeight: отрицательное значение означает top-down DIB (первая строка — верх).
        - biPlanes: всегда 1.
        - biBitCount: 32 бита на пиксель (RGBA/ARGB).
        - biCompression: BI_RGB (не сжатый).
    */
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = h; // top-down DIB — важно для корректной ориентации
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // Выводим буфер в окно с помощью StretchDIBits.
    // Здесь исходные размеры (src) совпадают с размерами целевой области (dst),
    // поэтому фактически происходит прямая копия.
    ::StretchDIBits(dc,
        0, 0, w, h,    // область назначения в окне: x,y,width,height
        0, 0, w, h,    // область источника в буфере: x,y,width,height
        pixels.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);

    // Освобождаем контекст устройства
    ::ReleaseDC(win, dc);

    // Информируем пользователя (на английском)
    std::cout << "Done. Press Enter to exit...\n";
    std::cin.get();
    return 0;
}
