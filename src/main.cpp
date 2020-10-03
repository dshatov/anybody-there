#include <raylib.h>
//#include <bits/stdc++.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <queue>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#define ASSETS ""
#else
#define ASSETS "data/"
#endif

static constexpr int WINDOW_WIDTH = 960;
static constexpr int WINDOW_HEIGHT = 540;
static constexpr int LOGIC_SCREEN_WIDTH = WINDOW_WIDTH * 2;
static constexpr int LOGIC_SCREEN_HEIGHT = WINDOW_HEIGHT * 2;
static const Vector2 LOGIC_SCREEN_CENTER = {(float) LOGIC_SCREEN_WIDTH / 2, (float) LOGIC_SCREEN_HEIGHT / 2};

static std::vector<std::function<bool(void)> > CALLBACKS;

static Sound UP_SFX[10];
static Sound DOWN_SFX[10];
//static Music MUSIC;

static RenderTexture2D screen;

inline float sqr(float v) {
    return v * v;
}

inline float ssqr(float v) {
    return v * std::abs(v);
}

float dist2(const Vector2 &v1, const Vector2 &v2) {
    return sqr(v1.x - v2.x) + sqr(v1.y - v2.y);
}

template<typename A, typename B>
bool collide(A *a, B *b) {
    return sqr(a->radius() + b->radius()) > dist2(a->pos, b->pos);
}

template<typename T>
inline T coerceIn(T v, T from, T to) {
    return v < from ? from : to < v ? to : v;
}

static struct Player {
    Vector2 pos = LOGIC_SCREEN_CENTER;
    float additRadius = 0;
    int hp = 0;
    float deltaAddit = 0.3;

    [[nodiscard]] static float radius() {
        return 40.0f;
    }

    void update() {
//        float d = std::sqrt(1 + dist2(LOGIC_SCREEN_CENTER, pos));
//        float vel = coerceIn(2000.0f / d, 0, 10);
        float vel = 10;
        if (IsKeyDown(KEY_LEFT)) pos.x -= vel;
        else if (IsKeyDown(KEY_RIGHT)) pos.x += vel;
        if (IsKeyDown(KEY_UP)) pos.y -= vel;
        else if (IsKeyDown(KEY_DOWN)) pos.y += vel;

        pos.x -= coerceIn(ssqr(pos.x - LOGIC_SCREEN_CENTER.x) / 60000, -30.f, 30.f);
        pos.y -= coerceIn(ssqr(pos.y - LOGIC_SCREEN_CENTER.y) / 20000, -20.f, 20.f);

        additRadius += deltaAddit;
        if (additRadius > 30) {
            additRadius = 30;
            deltaAddit = -deltaAddit;
        }
        if (additRadius < 0) {
            additRadius = 0;
            deltaAddit = -deltaAddit;
        }
    };

    void draw() const {
        DrawLineBezier(pos, LOGIC_SCREEN_CENTER, 10.0f, RED);
        DrawCircleV(pos, radius(), RED);
        DrawCircleV(pos, float(5 + additRadius), MAROON);

        char hps[16];
        std::snprintf(hps, 16, "HP = %d", hp);
        DrawText(hps, 300, 300, 30, ORANGE);
    };

    void touch(bool good) {
        hp = coerceIn(hp + (good ? 1 : -1), 0, 10);
        if (good) PlaySound(UP_SFX[hp - 1]);
        else PlaySound(DOWN_SFX[hp]);

        deltaAddit *= (float(hp) + 1.f) * 0.3f / std::abs(deltaAddit);
    }
} PLAYER; // NOLINT(cert-err58-cpp)

struct Bullet {
    int goodPercent;
    float lifetime;
    bool good;
    float angle;
    float velScalar;
    bool touched;

    Vector2 pos = {0};
    Vector2 vel = {0};

    explicit Bullet(int goodPercent) { // NOLINT(cppcoreguidelines-pro-type-member-init)
        this->goodPercent = goodPercent;
        reset();
    }

    void reset() {
        lifetime = 660.0f + (float) GetRandomValue(60, 600);
        good = GetRandomValue(1, 100) <= this->goodPercent;
        angle = float(GetRandomValue(0, 359)) * DEG2RAD;
        velScalar = float(GetRandomValue(1000, 1500)) * 0.01f;
        touched = false;

        pos = {
            float(GetRandomValue(0, LOGIC_SCREEN_WIDTH)) + LOGIC_SCREEN_WIDTH * 1.5f,
            float(GetRandomValue(0, LOGIC_SCREEN_HEIGHT)) + LOGIC_SCREEN_HEIGHT * 1.5f
        };

        vel = {
            std::cos(angle) * velScalar,
            std::sin(angle) * velScalar,
        };
    }

    void update() {
        if (lifetime < 0) return;
        --lifetime;
        if (touched) lifetime -= 4;
        pos.x += vel.x;
        pos.y += vel.y;

        static float constexpr MARGIN = 30.f;
        if (pos.x < -MARGIN) pos.x = LOGIC_SCREEN_WIDTH + MARGIN;
        if (pos.y < -MARGIN) pos.y = LOGIC_SCREEN_WIDTH + MARGIN;
        if (pos.x > LOGIC_SCREEN_WIDTH + MARGIN) pos.x = -MARGIN;
        if (pos.y > LOGIC_SCREEN_WIDTH + MARGIN) pos.y = -MARGIN;

        if (!touched && lifetime > 70.f && collide(this, &PLAYER)) {
            touched = true;
            PLAYER.touch(good);
            lifetime = std::min(150.0f, lifetime);
        }
    };

    [[nodiscard]] float radius() const {
        return float(std::min(150.0f, lifetime) / 5.0f);
    }

    void draw() const {
        if (lifetime < 0) return;
        DrawCircleV(pos, radius(), good ? RED : SKYBLUE);
    };
};

static std::vector<Bullet> BULLETS;

static struct {
    bool show = true;

    void toggle() {
        show = !show;
    }

    void draw() const {
        if (show) {
            static const Color HELP_COLOR = (Color) {255, 203, 0, 192};
            DrawText(
                ""
                "Controls:\n\n"
                "ARROWS  --  move\n"
                "ESC  --  exit\n"
                "F  --  fullscreen\n"
                "H  --  toggle help\n"
                "",
                8, 8, 30, HELP_COLOR);
        }
    }
} HELP;

static struct {
    std::queue<std::pair<std::string, std::function<void(void)> > > messages;

    void next() {
        if (messages.empty()) return;
        auto second = messages.front().second;
        messages.pop();
        second();
    }

    void add(const std::string &msg) {
        messages.emplace(msg, []() {});
    }

    void add(const std::string &msg, const std::function<void(void)> &callback) {
        messages.emplace(msg, callback);
    }

    void update() {
        if (IsKeyPressed(KEY_SPACE)) next();
    }

    void clear() {
        while (!messages.empty()) messages.pop();
    }

    void draw() const {
        if (messages.empty()) return;

        static const Color MESSAGE_COLOR = {255, 255, 255, 192};
        static const Color MSG_BOX_COLOR = {130, 130, 130, 192};
        static const Color HINT_COLOR = {190, 190, 190, 192};

        static const Rectangle MSG_BOX_RECT = {24, 868, 1872, 188};

        DrawRectangleRounded(MSG_BOX_RECT, 0.3, 32, MSG_BOX_COLOR);
        DrawText(
            messages.front().first.c_str(),
            56, 900, 30, MESSAGE_COLOR
        );
        DrawText(
            "\n\nPress SPACE",
            1650, 900, 30, HINT_COLOR
        );
    }
} MESSAGE; // NOLINT(cert-err58-cpp)

//----------------------------------------------------------------------------------------------------------------------

static bool ALL_BULLETS_TOUCHED = false;

void touchAllBullets() {
    if (ALL_BULLETS_TOUCHED) return;
    ALL_BULLETS_TOUCHED = true;
    for (auto &obj : BULLETS) {
        obj.touched = true;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void startLevel01();

void startLevel02();

void startLevel03();

//----------------------------------------------------------------------------------------------------------------------

void startLevel(
    const std::vector<std::string> &startMsg,
    const std::vector<std::string> &endMsg,
    int bulletsCount,
    int goodPercent,
    const std::function<void(void)> &next
) {
    ALL_BULLETS_TOUCHED = false;
    BULLETS.clear();
    CALLBACKS.clear();
    MESSAGE.clear();
    Player p;
    std::swap(p, PLAYER);

    for (int i = 0; i < startMsg.size() - 1; ++i) {
        MESSAGE.add(startMsg[i]);
    }
    MESSAGE.add(startMsg.back(), [endMsg, bulletsCount, goodPercent, next]() {
        auto endMsgCopy = endMsg; // NOLINT(performance-unnecessary-copy-initialization)
        for (int i = 0; i < bulletsCount; ++i) {
            BULLETS.emplace_back(goodPercent);
            Bullet &obj = BULLETS[BULLETS.size() - 1];
        }

        CALLBACKS.emplace_back([]() {
            if (PLAYER.hp == 10) {
                touchAllBullets();
            }
            return false;
        });

        CALLBACKS.emplace_back([endMsgCopy, next]() {
            auto endMsgCopy2 = endMsgCopy;
            if (BULLETS.empty()) {
                CALLBACKS.clear();
                for (int i = 0; i < endMsgCopy2.size() - 1; ++i) {
                    MESSAGE.add(endMsgCopy2[i]);
                }
                MESSAGE.add(endMsgCopy2.back(), next);
            }
            return true;
        });
    });
}

void startLevel01() {
    std::vector<std::string> startMsg;
    startMsg.emplace_back("Hello...");
    startMsg.emplace_back("Anybody there?");
    startMsg.emplace_back("LEVEL 1");

    std::vector<std::string> endMsg;
    endMsg.emplace_back("Fine! Next level...");

    startLevel(startMsg, endMsg, 30, 100, startLevel02);
}


void startLevel02() {
    std::vector<std::string> startMsg;
    startMsg.emplace_back("Well...");
    startMsg.emplace_back("LEVEL 2");

    std::vector<std::string> endMsg;
    endMsg.emplace_back("Fine! Next level...");

    startLevel(startMsg, endMsg, 15, 50, startLevel03);
}

void startLevel03() {
    std::vector<std::string> startMsg;
    startMsg.emplace_back("Ok dude...");
    startMsg.emplace_back("LEVEL 3");

    std::vector<std::string> endMsg;
    endMsg.emplace_back("YEAAAH...");

    startLevel(startMsg, endMsg, 30, 30, startLevel01);
}

void startLevel00() {
    MESSAGE.add("Hint: You can press [H]", []() {
        InitAudioDevice();
        UP_SFX[0] = LoadSound(ASSETS "1_up01.mp3");
        UP_SFX[1] = LoadSound(ASSETS "2_up02.mp3");
        UP_SFX[2] = LoadSound(ASSETS "3_up03.mp3");
        UP_SFX[3] = LoadSound(ASSETS "4_up04.mp3");
        UP_SFX[4] = LoadSound(ASSETS "5_up05.mp3");
        UP_SFX[5] = LoadSound(ASSETS "6_up06.mp3");
        UP_SFX[6] = LoadSound(ASSETS "7_up07.mp3");
        UP_SFX[7] = LoadSound(ASSETS "8_up08.mp3");
        UP_SFX[8] = LoadSound(ASSETS "9_up09.mp3");
        UP_SFX[9] = LoadSound(ASSETS "10_up10.mp3");

        DOWN_SFX[0] = LoadSound(ASSETS "11_down01.mp3");
        DOWN_SFX[1] = LoadSound(ASSETS "12_down02.mp3");
        DOWN_SFX[2] = LoadSound(ASSETS "13_down03.mp3");
        DOWN_SFX[3] = LoadSound(ASSETS "14_down04.mp3");
        DOWN_SFX[4] = LoadSound(ASSETS "15_down05.mp3");
        DOWN_SFX[5] = LoadSound(ASSETS "16_down06.mp3");
        DOWN_SFX[6] = LoadSound(ASSETS "17_down07.mp3");
        DOWN_SFX[7] = LoadSound(ASSETS "18_down08.mp3");
        DOWN_SFX[8] = LoadSound(ASSETS "19_down09.mp3");
        DOWN_SFX[9] = LoadSound(ASSETS "20_down10.mp3");

        CALLBACKS.emplace_back([]() {
            if (IsKeyPressed(KEY_Z)) PlaySound(UP_SFX[0]);
            if (IsKeyPressed(KEY_X)) PlaySound(UP_SFX[1]);
            if (IsKeyPressed(KEY_C)) PlaySound(UP_SFX[2]);
            if (IsKeyPressed(KEY_V)) PlaySound(UP_SFX[3]);
            if (IsKeyPressed(KEY_B)) PlaySound(UP_SFX[4]);
            if (IsKeyPressed(KEY_N)) PlaySound(UP_SFX[5]);
            if (IsKeyPressed(KEY_M)) PlaySound(UP_SFX[6]);
            return false;
        });
//        MUSIC = LoadMusicStream(ASSETS "01_metro.mp3");
//        PlayMusicStream(MUSIC);

        MESSAGE.add("PRESS ZXCVBNNM to test sound", []() {
            startLevel01();
        });
    });
}

//----------------------------------------------------------------------------------------------------------------------

void toggleFullscreen() {
    if (IsWindowFullscreen()) {
        ToggleFullscreen();
        SetWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    } else {
        SetWindowSize(GetMonitorWidth(0), GetMonitorHeight(0));
        ToggleFullscreen();
    }
}

void update() {
    if (IsKeyPressed(KEY_F)) toggleFullscreen();
    if (IsKeyPressed(KEY_H)) HELP.toggle();
    if (IsKeyPressed(KEY_R)) startLevel01();

    MESSAGE.update();
//    if (MUSIC.sampleCount) UpdateMusicStream(MUSIC);

    bool anyAlive = false;
    for (auto &obj : BULLETS) {
        obj.update();
        anyAlive = anyAlive || 0 < obj.lifetime;
        if (obj.lifetime < 0 && !ALL_BULLETS_TOUCHED) {
            obj.reset();
        }
    }
    if (ALL_BULLETS_TOUCHED && !anyAlive) {
        BULLETS.clear();
    }

    PLAYER.update();

    for (auto &f : CALLBACKS) {
        if (f()) {
            break;
        }
    }
}

void drawScreenTexture() {
    float scale = std::min(
        (float) GetScreenWidth() / LOGIC_SCREEN_WIDTH,
        (float) GetScreenHeight() / LOGIC_SCREEN_HEIGHT
    );
    DrawTexturePro(
        screen.texture,
        (Rectangle) {
            0.0f, 0.0f, (float) screen.texture.width, (float) -screen.texture.height
        },
        (Rectangle) {
            ((float) GetScreenWidth() - ((float) LOGIC_SCREEN_WIDTH * scale)) * 0.5f,
            ((float) GetScreenHeight() - ((float) LOGIC_SCREEN_HEIGHT * scale)) * 0.5f,
            (float) LOGIC_SCREEN_WIDTH * scale, (float) LOGIC_SCREEN_HEIGHT * scale
        },
        (Vector2) {0, 0}, 0.0f, WHITE
    );
}

void draw() {
    BeginDrawing();
    ClearBackground(BLACK);
    BeginTextureMode(screen);

    //----------------------------------------------------------------

    ClearBackground(DARKGRAY);
    PLAYER.draw();
    for (auto &obj : BULLETS) {
        obj.draw();
    }
    HELP.draw();
    MESSAGE.draw();

    //----------------------------------------------------------------

    EndTextureMode();
    drawScreenTexture();
    EndDrawing();
}

//----------------------------------------------------------------------------------------------------------------------

void init() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE); // NOLINT(hicpp-signed-bitwise)
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "LD 47 Entry");
    SetTargetFPS(60);
//    while (!IsKeyPressed(KEY_SPACE)) {
//        BeginDrawing();
//        ClearBackground(BLACK);
//        DrawText(
//            "PRESS SPACE TO START",
//            32, 32, 30, WHITE
//        );
//        EndDrawing();
//    }

    screen = LoadRenderTexture(LOGIC_SCREEN_WIDTH, LOGIC_SCREEN_HEIGHT);
    SetTextureFilter(screen.texture, FILTER_TRILINEAR);
    //    toggleFullscreen();
//    SetExitKey(0);
}

void updateAndDraw() {
    update();
    draw();
}

int main() {
    init();

    startLevel00();
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(updateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        updateAndDraw();
    }

    UnloadRenderTexture(screen);
//    UnloadSound(UP_SFX[0]); // TODO!
    CloseAudioDevice();
    CloseWindow();
    return 0;
#endif
}