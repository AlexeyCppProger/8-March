// Добавлена возможность перемещения в третьей плоскости при помощи кнопок PageUp и PageDown
// PageUp - приближение, PageDown - отдаление
// Приложение неоптимизированно, при желании можно зяняться этим добавив асинхронность/многопоточность или сделав, чтобы каждый раз выводились не все пиксели сразу (при моём разрешении в 2k это повлияет очень сильно)

#define NOMINMAX
#include <Windows.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <chrono>
#include <thread>

/*
  3D Heart viewer — исправленная версия.
  Управление:
    - ЛКМ + перетаскивание: orbit
    - Arrow keys: rotate
    - PageUp/PageDown: zoom
    - Esc: exit
*/

/* ---------------- Простая векторная структура ---------------- */
struct vec3 { float x, y, z; vec3() : x(0), y(0), z(0) {} vec3(float a, float b, float c) : x(a), y(b), z(c) {} };
inline vec3 operator+(vec3 const& a, vec3 const& b) { return vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline vec3 operator-(vec3 const& a, vec3 const& b) { return vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline vec3 operator-(vec3 const& a) { return vec3(-a.x, -a.y, -a.z); } // унарный минус
inline vec3 operator*(vec3 const& a, float s) { return vec3(a.x * s, a.y * s, a.z * s); }
inline vec3 operator*(float s, vec3 const& a) { return vec3(a.x * s, a.y * s, a.z * s); }
inline vec3 operator*(vec3 const& a, vec3 const& b) { return vec3(a.x * b.x, a.y * b.y, a.z * b.z); } // поэлементное умножение
inline float dot(vec3 const& a, vec3 const& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline vec3 cross(vec3 const& a, vec3 const& b) { return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline float length(vec3 const& v) { return std::sqrt(dot(v, v)); }
inline vec3 normalize(vec3 const& v) { float L = length(v); if (L == 0.f) return vec3(0.f, 1.f, 0.f); return v * (1.f / L); }

/* ---------------- Функция сердца (неявная поверхность) ---------------- */
float f(vec3 p)
{
    vec3 pp = p * p;
    vec3 ppp = pp * p;
    float a = pp.x + 2.25f * pp.y + pp.z - 1.0f;
    return a * a * a - (pp.x + 0.1125f * pp.y) * ppp.z;
}

/* ---------------- Нормаль численным градиентом ---------------- */
vec3 numericNormal(vec3 p)
{
    const float eps = 1e-3f;
    float fx = f(vec3(p.x + eps, p.y, p.z)) - f(vec3(p.x - eps, p.y, p.z));
    float fy = f(vec3(p.x, p.y + eps, p.z)) - f(vec3(p.x, p.y - eps, p.z));
    float fz = f(vec3(p.x, p.y, p.z + eps)) - f(vec3(p.x, p.y, p.z - eps));
    vec3 g = vec3(fx, fy, fz) * (0.5f / eps);
    return normalize(g);
}

/* ---------------- Вспомогательные функции для пересечений ---------------- */
/* пересечение луча со сферой (аналитически). Возвращает t в [tNear,tFar] или -1.0f */
float intersectSphere(vec3 origin, vec3 dir, vec3 center, float radius, float tNear, float tFar)
{
    vec3 oc = origin - center;
    float b = dot(oc, dir);
    float c = dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0f) return -1.0f;
    float sq = std::sqrt(disc);
    float t1 = -b - sq;
    float t2 = -b + sq;
    if (t1 >= tNear && t1 <= tFar) return t1;
    if (t2 >= tNear && t2 <= tFar) return t2;
    return -1.0f;
}

/* пересечение луча с неявной поверхностью сердца: скан + бисекция */
float intersectRayHeart(vec3 origin, vec3 dir, float tNear, float tFar)
{
    const int maxSamples = 200;
    const int bisectIters = 30;
    float step = (tFar - tNear) / float(maxSamples);
    float prevT = tNear;
    float prevVal = f(origin + dir * prevT);
    for (int i = 1; i <= maxSamples; ++i)
    {
        float t = tNear + step * i;
        float val = f(origin + dir * t);
        if (val <= 0.0f && prevVal > 0.0f)
        {
            float a = prevT, b = t;
            for (int it = 0; it < bisectIters; ++it)
            {
                float m = 0.5f * (a + b);
                float vm = f(origin + dir * m);
                if (vm <= 0.0f) b = m; else a = m;
            }
            return 0.5f * (a + b);
        }
        prevT = t; prevVal = val;
    }
    return -1.0f;
}

/* ---------------- Шейдинг ---------------- */
/* reflect объявлен до shadeAt */
inline vec3 reflectVec(vec3 I, vec3 N) { return I - N * (2.0f * dot(I, N)); }

vec3 shadeAt(vec3 p, vec3 n, vec3 viewDir)
{
    vec3 lightDir = normalize(vec3(-1.f, 1.f, 1.f));
    float diff = std::max(dot(n, lightDir), 0.0f);
    float diffuse = diff * 0.9f + 0.1f;
    float spec = std::pow(std::max(dot(reflectVec(-lightDir, n), viewDir), 0.0f), 64.0f);
    float rim = std::pow(1.0f - std::max(dot(n, viewDir), 0.0f), 2.0f) * 0.25f;
    vec3 base(1.f, 0.15f, 0.25f);
    vec3 col = base * diffuse + vec3(1.0f, 1.0f, 1.0f) * spec + vec3(0.6f, 0.6f, 0.6f) * rim;
    return col;
}

/* ---------------- Преобразование цвета в 32-bit BGRA ---------------- */
float clampf(float v, float lo, float hi) { if (v < lo) return lo; if (v > hi) return hi; return v; }
int my_lround(float f) { int i = static_cast<int>(f); return (f - i < 0.5f) ? i : (i + 1); }
std::uint32_t toBGRA(vec3 color)
{
    const std::uint32_t r = 0xff & static_cast<std::uint32_t>(my_lround(clampf(color.x, 0.0f, 1.f) * 255.f));
    const std::uint32_t g = 0xff & static_cast<std::uint32_t>(my_lround(clampf(color.y, 0.0f, 1.f) * 255.f));
    const std::uint32_t b = 0xff & static_cast<std::uint32_t>(my_lround(clampf(color.z, 0.0f, 1.f) * 255.f));
    return (0xFF000000u) | (r << 16) | (g << 8) | (b);
}

/* ---------------- Рендер кадра ---------------- */
void render(vec3 position, vec3 target, vec3 worldUp, float fov, std::vector<std::uint32_t>& pixels, int w, int h)
{
    vec3 forward = normalize(target - position);
    // если forward почти коллинеарен с worldUp, подменим up, чтобы избежать вырождения
    if (std::fabs(dot(forward, worldUp)) > 0.999f) worldUp = vec3(1.f, 0.f, 0.f);
    vec3 right = normalize(cross(forward, worldUp));
    vec3 camUp = cross(right, forward);

    float aspect = float(w) / float(h);
    float tanHalfFov = std::tan(fov * 0.5f);

    const float tNear = 0.01f;
    const float tFar = 20.0f;

    // маркеры осей (маленькие сферы)
    struct Marker { vec3 c; float r; vec3 col; };
    Marker markers[3] = {
        { vec3(1.0f, 0.0f, 0.0f), 0.06f, vec3(0.9f, 0.1f, 0.1f) }, // X
        { vec3(0.0f, 1.0f, 0.0f), 0.06f, vec3(0.1f, 0.9f, 0.1f) }, // Y
        { vec3(0.0f, 0.0f, 1.0f), 0.06f, vec3(0.1f, 0.2f, 0.9f) }  // Z
    };

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            float ndcX = ((x + 0.5f) / float(w)) * 2.0f - 1.0f;
            float ndcY = 1.0f - ((y + 0.5f) / float(h)) * 2.0f;
            float sx = ndcX * aspect * tanHalfFov;
            float sy = ndcY * tanHalfFov;
            vec3 dir = normalize(forward + right * sx + camUp * sy);

            // тест маркеров (быстро)
            float bestT = tFar + 1.0f;
            int hitKind = 0; // 0 - none, 1 - heart, 2..4 - markers
            int hitMarkerIdx = -1;
            for (int m = 0; m < 3; ++m)
            {
                float ts = intersectSphere(position, dir, markers[m].c, markers[m].r, tNear, tFar);
                if (ts > 0.0f && ts < bestT)
                {
                    bestT = ts; hitKind = 2 + m; hitMarkerIdx = m;
                }
            }

            // тест сердца
            float tHeart = intersectRayHeart(position, dir, tNear, tFar);
            if (tHeart > 0.0f && tHeart < bestT)
            {
                bestT = tHeart; hitKind = 1;
            }

            if (hitKind != 0)
            {
                vec3 p = position + dir * bestT;
                vec3 n;
                vec3 col;
                if (hitKind == 1)
                {
                    n = numericNormal(p);
                    col = shadeAt(p, n, normalize(position - p));
                }
                else
                {
                    int idx = hitMarkerIdx;
                    n = normalize(p - markers[idx].c);
                    float diff = std::max(dot(n, normalize(vec3(-1.f, 1.f, 1.f))), 0.0f);
                    col = markers[idx].col * (0.5f + 0.5f * diff);
                }
                pixels[y * w + x] = toBGRA(col);
            }
            else
            {
                // фон + сетка на плоскости y = -1
                vec3 bgColor = vec3(0.9f, 0.8f, 0.7f) * (1.0f - 0.15f * (sx * sx + sy * sy));
                if (std::fabs(dir.y) > 1e-5f)
                {
                    float tplane = (-1.0f - position.y) / dir.y;
                    if (tplane > 0.0f && tplane < tFar)
                    {
                        vec3 pp = position + dir * tplane;
                        // grid: линии каждые 0.25
                        float gx = std::fabs(std::fmod(pp.x, 0.25f));
                        if (gx > 0.125f) gx = 0.25f - gx;
                        float gz = std::fabs(std::fmod(pp.z, 0.25f));
                        if (gz > 0.125f) gz = 0.25f - gz;
                        float line = (gx < 0.01f || gz < 0.01f) ? 0.0f : 1.0f;
                        float majorX = std::fabs(std::fmod(pp.x, 1.0f)); if (majorX > 0.5f) majorX = 1.0f - majorX;
                        float majorZ = std::fabs(std::fmod(pp.z, 1.0f)); if (majorZ > 0.5f) majorZ = 1.0f - majorZ;
                        if (majorX < 0.02f || majorZ < 0.02f) line *= 0.35f;
                        float att = 1.0f / (1.0f + 0.2f * tplane * tplane);
                        vec3 gridCol = vec3(0.3f, 0.3f, 0.35f) * (1.0f - line) * att + vec3(0.05f, 0.05f, 0.06f) * (1.0f - att);
                        pixels[y * w + x] = toBGRA(bgColor * 0.6f + gridCol);
                        continue;
                    }
                }
                pixels[y * w + x] = toBGRA(bgColor);
            }
        }
    }
}

/* ---------------- main: окно, ввод, цикл ---------------- */
int main()
{
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // РАСШИРЯЕМ РАЗМЕР КОНСОЛИ ДО РАЗМЕРА ЭКРАНА
     
    // Получаем дескрипторы
    HWND hWnd = GetConsoleWindow();
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    // Максимизируем окно (альтернатива Alt+Enter)
    if (hWnd) ShowWindow(hWnd, SW_MAXIMIZE);

    // Получаем максимально возможный размер окна в символах для текущего шрифта
    COORD largest = GetLargestConsoleWindowSize(hOut);
    if (largest.X == 0 || largest.Y == 0) {
        printf("Не удалось получить максимальный размер консоли.\n");
        return 1;
    }

    // Устанавливаем размер буфера в этот максимум (буфер должен быть >= окну)
    if (!SetConsoleScreenBufferSize(hOut, largest)) {
        printf("SetConsoleScreenBufferSize failed: %u\n", GetLastError());
        // можно попытаться продолжить
    }

    // Устанавливаем окно консоли на весь буфер
    SMALL_RECT sr;
    sr.Left = 0; sr.Top = 0; sr.Right = largest.X - 1; sr.Bottom = largest.Y - 1;
    if (!SetConsoleWindowInfo(hOut, TRUE, &sr)) {
        printf("SetConsoleWindowInfo failed: %u\n", GetLastError());
    }

    // Теперь получаем актуальную размерность окна/буфера
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) {
        printf("GetConsoleScreenBufferInfo failed: %u\n", GetLastError());
        return 1;
    }

    int width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    // Пример: очистить экран и вывести информацию по всему экрану
    DWORD written;
    FillConsoleOutputCharacter(hOut, ' ', (DWORD)(largest.X * largest.Y), { 0,0 }, &written);
    FillConsoleOutputAttribute(hOut, csbi.wAttributes, (DWORD)(largest.X * largest.Y), { 0,0 }, &written);
    SetConsoleCursorPosition(hOut, { 0,0 });

    printf("Console maximized: width=%d, height=%d\n", width, height);
    printf("Теперь используйте width и height для динамической отрисовки.\n");


    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    HWND win = ::GetConsoleWindow();
    if (!win) { std::cerr << "GetConsoleWindow() returned NULL\n"; return 1; }

    WINDOWINFO wi = {}; wi.cbSize = sizeof(WINDOWINFO);
    if (!::GetWindowInfo(win, &wi)) { std::cerr << "GetWindowInfo failed\n"; return 1; }

    HDC dc = ::GetDC(win);
    if (!dc) { std::cerr << "GetDC failed\n"; return 1; }

    int w = wi.rcClient.right - wi.rcClient.left;
    int h = wi.rcClient.bottom - wi.rcClient.top;
    if (w <= 0 || h <= 0) { std::cerr << "Invalid client size\n"; ::ReleaseDC(win, dc); return 1; }

    std::vector<std::uint32_t> pixels;
    try { pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h)); }
    catch (...) { std::cerr << "Failed to allocate pixel buffer\n"; ::ReleaseDC(win, dc); return 1; }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::cout << "3D Heart viewer (orbit with mouse)\n";
    std::cout << "Left mouse + drag: orbit. Arrow keys: rotate. PageUp/PageDown: zoom. Esc: exit.\n";
    std::cout << "Press Enter to start...\n";
    std::cin.get();

    // камера: сферические параметры
    float azimuth = 0.0f;
    float elevation = 0.2f;
    float distance = 2.5f;
    const float minDistance = 0.4f, maxDistance = 10.0f;
    const float rotSpeed = 1.6f;
    const float elevSpeed = 1.2f;
    const float zoomSpeed = 1.8f;
    const float fov = 45.0f * 3.14159265f / 180.0f;

    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    POINT prevPt = {}; bool prevPressed = false;
    const float mouseSens = 0.0055f;

    bool running = true;
    while (running)
    {
        auto now = clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { running = false; break; }

        // клавиатура
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)  azimuth -= rotSpeed * dt;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) azimuth += rotSpeed * dt;
        if (GetAsyncKeyState(VK_UP) & 0x8000)    elevation += elevSpeed * dt;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)  elevation -= elevSpeed * dt;
        if (GetAsyncKeyState(VK_PRIOR) & 0x8000) { distance -= zoomSpeed * dt; if (distance < minDistance) distance = minDistance; }
        if (GetAsyncKeyState(VK_NEXT) & 0x8000) { distance += zoomSpeed * dt; if (distance > maxDistance) distance = maxDistance; }

        // мышь
        bool pressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        POINT cur; ::GetCursorPos(&cur);
        ::ScreenToClient(win, &cur);
        if (pressed && prevPressed)
        {
            int dx = cur.x - prevPt.x;
            int dy = cur.y - prevPt.y;
            azimuth -= dx * mouseSens;
            elevation += dy * mouseSens;
        }
        prevPt = cur;
        prevPressed = pressed;

        // нормировка azimuth
        if (azimuth > 3.14159265f) azimuth -= 2.0f * 3.14159265f;
        if (azimuth < -3.14159265f) azimuth += 2.0f * 3.14159265f;

        // позиция камеры (сферические -> декарт)
        float ca = std::cos(azimuth), sa = std::sin(azimuth);
        float ce = std::cos(elevation), se = std::sin(elevation);
        vec3 camPos = vec3(distance * ca * ce, distance * se, distance * sa * ce);
        vec3 target = vec3(0.f, 0.f, 0.f);
        vec3 worldUp = vec3(0.f, 1.f, 0.f);

        render(camPos, target, worldUp, fov, pixels, w, h);

        ::StretchDIBits(dc, 0, 0, w, h, 0, 0, w, h, pixels.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    ::ReleaseDC(win, dc);
    std::cout << "Exiting...\n";
    return 0;
}
