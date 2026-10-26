#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using namespace std;
using Clock = chrono::steady_clock;

constexpr int FIELD_W = 10;
constexpr int FIELD_H = 20;
constexpr int FIELD_TOP = 4;
constexpr int FIELD_LEFT = 3;

enum class Filter { Green = 0, Amber = 1, Ice = 2, Mono = 3 };
Filter currentFilter = Filter::Green;
bool scanlines = false; // Off by default: clearer image and less visual strain.
bool showGhost = false;
bool gameOver = false;
bool paused = false;
int score = 0;
int linesCleared = 0;
int level = 1;

HANDLE outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
vector<vector<int>> grid(FIELD_H, vector<int>(FIELD_W, 0));

struct Palette {
    WORD main;
    WORD dim;
    WORD bright;
    WORD inverse;
    const char* name;
};

Palette palette() {
    switch (currentFilter) {
        case Filter::Amber:
            return { FOREGROUND_RED | FOREGROUND_GREEN,
                     FOREGROUND_RED | FOREGROUND_GREEN,
                     FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
                     BACKGROUND_RED | BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
                     "AMBER CRT" };
        case Filter::Ice:
            return { FOREGROUND_GREEN | FOREGROUND_BLUE,
                     FOREGROUND_GREEN | FOREGROUND_BLUE,
                     FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
                     BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
                     "ICE BLUE" };
        case Filter::Mono:
            return { FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
                     FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
                     FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
                     BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
                     "MONOCHROME" };
        default:
            return { FOREGROUND_GREEN,
                     FOREGROUND_GREEN,
                     FOREGROUND_GREEN | FOREGROUND_INTENSITY,
                     BACKGROUND_GREEN | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
                     "GREEN PHOSPHOR" };
    }
}

const array<array<const char*, 4>, 7> SHAPES = {{
    {{ "....", "XXXX", "....", "...." }},
    {{ ".XX.", ".XX.", "....", "...." }},
    {{ ".X..", "XXX.", "....", "...." }},
    {{ ".XX.", "XX..", "....", "...." }},
    {{ "XX..", ".XX.", "....", "...." }},
    {{ "X...", "XXX.", "....", "...." }},
    {{ "..X.", "XXX.", "....", "...." }}
}};

struct Piece { int type = 0; int rotation = 0; int x = 3; int y = 0; };
Piece active;
int nextType = 0;
array<int, 7> bag{};
int bagIndex = 7;

int nextFromBag() {
    if (bagIndex >= 7) {
        iota(bag.begin(), bag.end(), 0);
        for (int i = 6; i > 0; --i) {
            const int j = rand() % (i + 1);
            swap(bag[i], bag[j]);
        }
        bagIndex = 0;
    }
    return bag[bagIndex++];
}

array<string, 4> shapeFor(int type, int rotation) {
    array<string, 4> out;
    for (int i = 0; i < 4; ++i) out[i] = SHAPES[type][i];
    for (int r = 0; r < rotation % 4; ++r) {
        array<string, 4> rotated = { "....", "....", "....", "...." };
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                rotated[x][3 - y] = out[y][x];
        out = rotated;
    }
    return out;
}

bool collides(const Piece& p) {
    const auto s = shapeFor(p.type, p.rotation);
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
        if (s[y][x] != 'X') continue;
        const int gx = p.x + x, gy = p.y + y;
        if (gx < 0 || gx >= FIELD_W || gy >= FIELD_H) return true;
        if (gy >= 0 && grid[gy][gx]) return true;
    }
    return false;
}

void spawnPiece() {
    active = Piece{ nextType, 0, 3, 0 };
    nextType = nextFromBag();
    if (collides(active)) gameOver = true;
}

void lockPiece() {
    const auto s = shapeFor(active.type, active.rotation);
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
        if (s[y][x] != 'X') continue;
        const int gx = active.x + x, gy = active.y + y;
        if (gy >= 0 && gy < FIELD_H && gx >= 0 && gx < FIELD_W) grid[gy][gx] = 1;
    }

    int cleared = 0;
    for (int y = FIELD_H - 1; y >= 0; --y) {
        if (all_of(grid[y].begin(), grid[y].end(), [](int v) { return v != 0; })) {
            grid.erase(grid.begin() + y);
            grid.insert(grid.begin(), vector<int>(FIELD_W, 0));
            ++cleared;
            ++y;
        }
    }
    const int points[] = { 0, 100, 300, 500, 800 };
    if (cleared > 0) {
        score += points[cleared] * level;
        linesCleared += cleared;
        level = linesCleared / 10 + 1;
    }
    spawnPiece();
}

struct Canvas {
    int width;
    int height;
    vector<CHAR_INFO> cells;
    Palette colors;

    Canvas(int w, int h, Palette p) : width(w), height(h), cells(static_cast<size_t>(w) * h), colors(p) {
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            auto& cell = cells[static_cast<size_t>(y) * width + x];
            cell.Char.AsciiChar = ' ';
            cell.Attributes = colors.dim;
        }
    }

    void put(int x, int y, char c, WORD attr = 0) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        auto& cell = cells[static_cast<size_t>(y) * width + x];
        cell.Char.AsciiChar = c;
        cell.Attributes = attr ? attr : colors.main;
    }

    void text(int x, int y, const string& s, WORD attr = 0) {
        for (size_t i = 0; i < s.size(); ++i) put(x + static_cast<int>(i), y, s[i], attr ? attr : colors.main);
    }

    void hline(int x, int y, int len, char c = '-', WORD attr = 0) {
        for (int i = 0; i < len; ++i) put(x + i, y, c, attr ? attr : colors.dim);
    }

    void box(int x, int y, int w, int h, WORD attr = 0) {
        if (w < 2 || h < 2) return;
        const WORD a = attr ? attr : colors.dim;
        put(x, y, '+', a); put(x + w - 1, y, '+', a);
        put(x, y + h - 1, '+', a); put(x + w - 1, y + h - 1, '+', a);
        hline(x + 1, y, w - 2, '-', a); hline(x + 1, y + h - 1, w - 2, '-', a);
        for (int yy = y + 1; yy < y + h - 1; ++yy) {
            put(x, yy, '|', a); put(x + w - 1, yy, '|', a);
        }
    }

    void applyScanlines() {
        if (!scanlines) return;
        // Simulate CRT horizontal retrace by dimming every second scan row.
        for (int y = 1; y < height; y += 2) {
            for (int x = 0; x < width; ++x) {
                auto& cell = cells[static_cast<size_t>(y) * width + x];
                const WORD fg = cell.Attributes & (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
                const WORD bg = cell.Attributes & (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY);
                cell.Attributes = bg | (fg & ~FOREGROUND_INTENSITY);
            }
        }
    }
};

void drawPiece(Canvas& c, const Piece& p, int left, int top, WORD attr) {
    const auto s = shapeFor(p.type, p.rotation);
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
        if (s[y][x] != 'X') continue;
        const int gx = p.x + x, gy = p.y + y;
        if (gx >= 0 && gx < FIELD_W && gy >= 0 && gy < FIELD_H) {
            c.put(left + gx * 2, top + gy, '[', attr);
            c.put(left + gx * 2 + 1, top + gy, ']', attr);
        }
    }
}

void drawField(Canvas& c, int left, int top) {
    c.box(left - 1, top - 1, FIELD_W * 2 + 2, FIELD_H + 2, c.colors.main);
    for (int y = 0; y < FIELD_H; ++y) for (int x = 0; x < FIELD_W; ++x) {
        if (grid[y][x]) {
            c.put(left + x * 2, top + y, '[', c.colors.bright);
            c.put(left + x * 2 + 1, top + y, ']', c.colors.bright);
        } else {
            c.put(left + x * 2, top + y, ' ', c.colors.dim);
            c.put(left + x * 2 + 1, top + y, ' ', c.colors.dim);
        }
    }

    if (!gameOver) {
        if (showGhost) {
            Piece ghost = active;
            while (true) {
                Piece candidate = ghost;
                ++candidate.y;
                if (collides(candidate)) break;
                ghost = candidate;
            }
            if (ghost.y != active.y) drawPiece(c, ghost, left, top, c.colors.dim);
        }
        drawPiece(c, active, left, top, c.colors.bright);
    }
}

void drawNext(Canvas& c, int x, int y) {
    const auto s = shapeFor(nextType, 0);
    for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col) {
        if (s[row][col] == 'X') {
            c.put(x + col * 2, y + row, '[', c.colors.bright);
            c.put(x + col * 2 + 1, y + row, ']', c.colors.bright);
        }
    }
}

void draw() {
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(outputHandle, &info)) return;

    // Always query the visible viewport again. The logical 10x20 game board
    // never changes size, only its placement / surrounding UI adapts.
    const int width = info.srWindow.Right - info.srWindow.Left + 1;
    const int height = info.srWindow.Bottom - info.srWindow.Top + 1;
    if (width < 1 || height < 1 || width > 1000 || height > 500) return;

    const Palette colors = palette();
    Canvas c(width, height, colors);

    if (width < 26 || height < 27) {
        c.text(1, 1, "CRT-84 TETRIS", colors.bright);
        c.hline(1, 3, max(0, width - 2), '-', colors.dim);
        c.text(1, 5, "WINDOW TOO SMALL", colors.main);
        c.text(1, 7, "Please enlarge the terminal to", colors.main);
        c.text(1, 8, "at least 26 columns x 27 rows.", colors.main);
        c.text(1, 10, "Your game state is safe.", colors.dim);
        c.text(1, 12, "F1-F4: screen filter", colors.dim);
        c.text(1, 13, "F5: scanlines (optional)", colors.dim);
        c.text(1, 14, "F6: ghost piece (optional)", colors.dim);
    } else {
        c.text(2, 0, "CRT-84  /  TETRIS", colors.bright);
        const string mode = string("DISPLAY: ") + colors.name;
        if (width >= 58 && static_cast<int>(mode.size()) + 3 < width)
            c.text(width - static_cast<int>(mode.size()) - 2, 0, mode, colors.main);
        c.hline(1, 2, width - 2, '-', colors.dim);

        // At wide sizes, keep the field left and place unboxed information to
        // its right. At narrow sizes, center the fixed-width field.
        const bool wide = width >= 62;
        const int fieldLeft = wide ? FIELD_LEFT : max(2, (width - (FIELD_W * 2 + 2)) / 2 + 1);
        const int fieldTop = FIELD_TOP;
        drawField(c, fieldLeft, fieldTop);

        if (wide) {
            const int panelX = fieldLeft + FIELD_W * 2 + 5;
            c.text(panelX, 4, "STATUS", colors.bright);
            c.hline(panelX, 5, min(20, width - panelX - 2), '-', colors.dim);
            c.text(panelX, 6, "SCORE  " + to_string(score), colors.main);
            c.text(panelX, 7, "LINES  " + to_string(linesCleared), colors.main);
            c.text(panelX, 8, "LEVEL  " + to_string(level), colors.main);

            c.text(panelX, 10, "NEXT PIECE", colors.bright);
            drawNext(c, panelX, 11);

            c.text(panelX, 16, "CONTROLS", colors.bright);
            c.hline(panelX, 17, min(20, width - panelX - 2), '-', colors.dim);
            c.text(panelX, 18, "LEFT/RIGHT  MOVE", colors.main);
            c.text(panelX, 19, "UP          ROTATE", colors.main);
            c.text(panelX, 20, "DOWN        SOFT DROP", colors.main);
            c.text(panelX, 21, "SPACE       HARD DROP", colors.main);
            c.text(panelX, 22, "P PAUSE     R RESTART", colors.main);
            c.text(panelX, 23, "F1-F4 FILTER", colors.main);
            c.text(panelX, 24, string("F5 SCANLINES ") + (scanlines ? "ON" : "OFF"), colors.main);
            c.text(panelX, 25, string("F6 GHOST     ") + (showGhost ? "ON" : "OFF"), colors.main);
            c.text(panelX, 26, "ESC EXIT", colors.main);
        } else {
            const int statusY = FIELD_TOP + FIELD_H + 2;
            if (statusY < height - 1) c.text(2, statusY, "SCORE " + to_string(score) + "  LINES " + to_string(linesCleared) + "  LV " + to_string(level), colors.main);
            if (statusY + 1 < height - 1) c.text(2, statusY + 1, "ARROWS MOVE / ROTATE   SPACE DROP", colors.main);
            if (statusY + 2 < height - 1) c.text(2, statusY + 2, "F1-F4 FILTER   F5 SCANLINES   F6 GHOST", colors.dim);
            if (statusY + 3 < height - 1) c.text(2, statusY + 3, "P PAUSE   R RESTART   ESC EXIT", colors.dim);
        }

        if (height >= 2) c.hline(1, height - 2, width - 2, '-', colors.dim);
        if (height >= 1) c.text(2, height - 1, "CRT DISPLAY  |  SCANLINES OFF BY DEFAULT", colors.dim);

        if (paused || gameOver) {
            const int boxW = min(width - 4, 36);
            const int boxH = 5;
            const int x = max(1, (width - boxW) / 2);
            const int y = max(1, (height - boxH) / 2);
            c.box(x, y, boxW, boxH, colors.bright);
            if (paused) {
                c.text(x + 3, y + 1, "PAUSED", colors.bright);
                c.text(x + 3, y + 3, "PRESS P TO RESUME", colors.main);
            } else {
                c.text(x + 3, y + 1, "GAME OVER", colors.bright);
                c.text(x + 3, y + 3, "PRESS R TO RESTART", colors.main);
            }
        }
    }

    c.applyScanlines();
    COORD sourceSize{ static_cast<SHORT>(width), static_cast<SHORT>(height) };
    COORD sourceOrigin{ 0, 0 };
    // The destination is the currently visible console viewport. If a resize
    // occurs between querying and writing, the next frame uses the new bounds.
    SMALL_RECT target = info.srWindow;
    WriteConsoleOutputA(outputHandle, c.cells.data(), sourceSize, sourceOrigin, &target);
}

void resetGame() {
    grid.assign(FIELD_H, vector<int>(FIELD_W, 0));
    score = 0;
    linesCleared = 0;
    level = 1;
    gameOver = false;
    paused = false;
    bagIndex = 7;
    nextType = nextFromBag();
    spawnPiece();
}

void setFilter(Filter f) { currentFilter = f; }

int main() {
    srand(static_cast<unsigned>(Clock::now().time_since_epoch().count()));
    SetConsoleTitleA("CRT-84 TETRIS // PHOSPHOR DISPLAY");
    SetConsoleOutputCP(437);

    CONSOLE_CURSOR_INFO cursorInfo{};
    if (GetConsoleCursorInfo(outputHandle, &cursorInfo)) {
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(outputHandle, &cursorInfo);
    }

    nextType = nextFromBag();
    spawnPiece();
    auto lastFall = Clock::now();
    bool running = true;

    while (running) {
        while (_kbhit()) {
            int key = _getch();
            if (key == 0 || key == 224) {
                const int ext = _getch();
                if (ext == 59) setFilter(Filter::Green);       // F1
                else if (ext == 60) setFilter(Filter::Amber);  // F2
                else if (ext == 61) setFilter(Filter::Ice);    // F3
                else if (ext == 62) setFilter(Filter::Mono);   // F4
                else if (ext == 63) scanlines = !scanlines;    // F5
                else if (ext == 64) showGhost = !showGhost;    // F6
                else if (!gameOver && !paused) {
                    Piece candidate = active;
                    if (ext == 75) --candidate.x;
                    else if (ext == 77) ++candidate.x;
                    else if (ext == 80) ++candidate.y;
                    else if (ext == 72) {
                        candidate.rotation = (candidate.rotation + 1) % 4;
                        bool fitted = false;
                        for (int offset : { 0, -1, 1, -2, 2 }) {
                            Piece kicked = candidate;
                            kicked.x += offset;
                            if (!collides(kicked)) { candidate = kicked; fitted = true; break; }
                        }
                        if (!fitted) candidate = active;
                    }
                    if (!collides(candidate)) active = candidate;
                    else if (ext == 80) lockPiece();
                }
            } else {
                const char c = static_cast<char>(tolower(key));
                if (key == 27) running = false;
                else if (c == 'r') { resetGame(); lastFall = Clock::now(); }
                else if (c == 'p' && !gameOver) { paused = !paused; lastFall = Clock::now(); }
                else if (key == ' ' && !gameOver && !paused) {
                    while (true) {
                        Piece candidate = active;
                        ++candidate.y;
                        if (collides(candidate)) break;
                        active = candidate;
                        score += 2;
                    }
                    lockPiece();
                }
            }
        }

        const auto now = Clock::now();
        const int interval = max(90, 650 - (level - 1) * 48);
        if (!paused && !gameOver && chrono::duration_cast<chrono::milliseconds>(now - lastFall).count() >= interval) {
            Piece candidate = active;
            ++candidate.y;
            if (!collides(candidate)) active = candidate;
            else lockPiece();
            lastFall = now;
        }
        draw();
        this_thread::sleep_for(chrono::milliseconds(16));
    }

    if (GetConsoleCursorInfo(outputHandle, &cursorInfo)) {
        cursorInfo.bVisible = TRUE;
        SetConsoleCursorInfo(outputHandle, &cursorInfo);
    }
    return 0;
}
