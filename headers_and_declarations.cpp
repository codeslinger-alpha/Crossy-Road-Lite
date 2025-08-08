#include <vector>
#include <queue>
#include <string>
#include <random>
#include <chrono>
#include <iostream>
#include "iFont.h"
int resource_id;
#define SLOPE 0.2679491924
#include "iGraphics.h"
 int HEIGHT = 1024;
 int WIDTH = 1000;
#define CELL 50 // multiple of 10
const int ROW = 1.0 * HEIGHT / CELL;
#define start_y 5

#define FPS 60
#define PLAYER_SPEED 120 // ms
const int player_fps = 12;
#define DEG(x) (x) * M_PI / 180

#include <unordered_map>
#include <string>
#include <SDL.h>
#include <SDL_mixer.h>
#include <cstdio>
#include <functional>


struct Button{
    int px, py, dx, dy;
    Image img;
};

namespace Audio
{
    Mix_Music *bgm = nullptr;
    std::unordered_map<int, Mix_Chunk *> sfxByChannelId;
    std::unordered_map<int, int> realChannelByChannelId;

    // Constants
    const int MUSIC_CHANNEL = -2;
    const int ALL_CHANNELS = -1;
    const int MAX_AUDIO_CHANNEL = 50;
    // Fire-and-forget deletion min-heap: sorted by delete_timestamp_ms
    static std::priority_queue<
        std::pair<Uint32, Mix_Chunk *>,
        std::vector<std::pair<Uint32, Mix_Chunk *>>,
        std::greater<std::pair<Uint32, Mix_Chunk *>>>
        deletionQueue;

    // Timer callback: call this periodically (e.g. via iSetTimer)
    void processDeletionQueue()
    {
        Uint32 now = SDL_GetTicks();
        while (!deletionQueue.empty() && deletionQueue.top().first <= now)
        {
            Mix_FreeChunk(deletionQueue.top().second);
            deletionQueue.pop();
        }
    }

    bool initAudio()
    {
        if (SDL_Init(SDL_INIT_AUDIO) < 0)
        {
            printf("SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
        {
            printf("Mix_OpenAudio failed: %s\n", Mix_GetError());
            return false;
        }
        return true;
    }

    void playAudio(int channel_id, bool loop, int volume, const char *filepath)
    {
        if (channel_id == MUSIC_CHANNEL)
        {
            if (bgm)
            {
                Mix_HaltMusic();
                Mix_FreeMusic(bgm);
                bgm = nullptr;
            }
            if (!filepath)
            {
                printf("No filepath for music!\n");
                return;
            }
            bgm = Mix_LoadMUS(filepath);
            if (!bgm)
            {
                printf("Failed to load music: %s\n", Mix_GetError());
                return;
            }
            Mix_VolumeMusic(volume);
            if (Mix_PlayMusic(bgm, loop ? -1 : 1) == -1)
                printf("Failed to play music: %s\n", Mix_GetError());
        }
        else if (channel_id == ALL_CHANNELS)
        {
            if (!filepath)
            {
                printf("No filepath for fire-and-forget SFX!\n");
                return;
            }
            Mix_Chunk *chunk = Mix_LoadWAV(filepath);
            if (!chunk)
            {
                printf("Failed to load: %s\n", Mix_GetError());
                return;
            }
            Mix_VolumeChunk(chunk, volume);

            int realCh = Mix_PlayChannel(-1, chunk, 0);
            if (realCh == -1)
            {
                Mix_FreeChunk(chunk);
                printf("No free channel: %s\n", Mix_GetError());
                return;
            }

            Uint32 lenBytes = chunk->alen;
            int durationMs = static_cast<int>(lenBytes * 1000 / (44100 * 2 * sizeof(Sint16)));
            Uint32 deleteTime = SDL_GetTicks() + durationMs;

            deletionQueue.push({deleteTime, chunk});
        }
        else
        {
            Mix_Chunk *chunk = nullptr;
            if (filepath)
            {
                chunk = Mix_LoadWAV(filepath);
                if (!chunk)
                {
                    printf("Failed to load: %s\n", Mix_GetError());
                    return;
                }
                auto old = sfxByChannelId.find(channel_id);
                if (old != sfxByChannelId.end())
                {
                    Mix_FreeChunk(old->second);
                    sfxByChannelId.erase(old);
                }
                sfxByChannelId[channel_id] = chunk;
                auto it = realChannelByChannelId.find(channel_id);
                if (it != realChannelByChannelId.end())
                    Mix_HaltChannel(it->second);
            }
            else
            {
                auto it = sfxByChannelId.find(channel_id);
                if (it == sfxByChannelId.end())
                {
                    printf("No loaded sound for this channel ID and no filepath!\n");
                    return;
                }
                chunk = it->second;
            }

            Mix_VolumeChunk(chunk, volume);
            int loops = loop ? -1 : 0;
            int real_channel;
            auto it2 = realChannelByChannelId.find(channel_id);
            if (it2 != realChannelByChannelId.end())
            {
                real_channel = it2->second;
                Mix_HaltChannel(real_channel);
            }
            else
            {
                real_channel = Mix_PlayChannel(-1, chunk, loops);
                if (real_channel == -1)
                {
                    printf("No free channel: %s\n", Mix_GetError());
                    return;
                }
                realChannelByChannelId[channel_id] = real_channel;
                return;
            }
            if (Mix_PlayChannel(real_channel, chunk, loops) == -1)
                printf("Failed to play: %s\n", Mix_GetError());
        }
    }

    void pauseAudio(int channel_id)
    {
        if (channel_id == MUSIC_CHANNEL)
        {
            Mix_PauseMusic();
        }
        else if (channel_id == ALL_CHANNELS)
        {
            Mix_Pause(-1);
        }
        else
        {
            auto it = realChannelByChannelId.find(channel_id);
            if (it != realChannelByChannelId.end())
            {
                Mix_Pause(it->second);
            }
        }
    }

    void resumeAudio(int channel_id)
    {
        if (channel_id == MUSIC_CHANNEL)
        {
            Mix_ResumeMusic();
        }
        else if (channel_id == ALL_CHANNELS)
        {
            Mix_Resume(-1);
        }
        else
        {
            auto it = realChannelByChannelId.find(channel_id);
            if (it != realChannelByChannelId.end())
            {
                Mix_Resume(it->second);
            }
        }
    }

    void stopAudio(int channel_id)
    {
        if (channel_id == MUSIC_CHANNEL)
        {
            Mix_HaltMusic();
        }
        else if (channel_id == ALL_CHANNELS)
        {
            Mix_HaltChannel(-1);
            realChannelByChannelId.clear();
        }
        else
        {
            auto it = realChannelByChannelId.find(channel_id);
            if (it != realChannelByChannelId.end())
            {
                Mix_HaltChannel(it->second);
                realChannelByChannelId.erase(it);
            }
        }
    }

    void cleanAudio()
    {
        if (bgm)
            Mix_FreeMusic(bgm);
        for (auto &p : sfxByChannelId)
            Mix_FreeChunk(p.second);
        sfxByChannelId.clear();
        realChannelByChannelId.clear();
        Mix_CloseAudio();
        SDL_Quit();
    }
}

Image TRUCK1, TRUCK2, CAR1, CAR2, ROCK, TRAIN, EAGLE, LILYPAD, MENU_IMAGE; // remember to free image when usin multiple DLC
bool flown_sound = false;
bool drown_sound = false;
bool eagle_sound = false;
bool flown_sound2 = false;
bool drown_sound2 = false;
bool eagle_sound2 = false;
struct MySprite
{
    std::pair<double, double> pos;
    std::vector<Image *> frames;
    int frame_id = 0;
};
MySprite splash,splash2; // splash is for player1, splash2 is for player2
std::vector<Image> TREE(4);

enum CollisionType
{
    None,
    Drown,
    FlownWithLog,
    Eagle,
    Vehicle
};
CollisionType Collision = None,Collision2 = None; // Collision2 is for player2
enum Type
{
    Street = 0,
    Water,
    Field
};
const int CAR_LEN = 2, TRUCK_LEN = 3, TRAIN_LEN = WIDTH / CELL * 2, LILYPAD_LEN = 1; // didn't add train bell, do it later. LEN should be atleast 1, as lilipod is 1
struct Bar
{
    int pos;
    double pospx;
    int size;
};

struct Line
{
    Type type;
    int dir;
    int speed_factor;
    std::vector<Bar> data;
};
enum Motion
{

    Up,
    Down,
    Right,
    Left,
    Dead
};
struct Player
{
    int x, y;
    double px, py;
    Motion motion;
    std::vector<Image> file = std::vector<Image>(5);
    int frame_no;
};
std::queue<Motion> keypress,keypress2; // keypress is for player1, keypress2 is for player2
namespace Vertical
{
    const double base_factor = (30.0 / 20) * FPS;
    int scroll_factor = base_factor;
    int V;
    void scroll();
}

namespace Timer
{
    int HScrollpx = -1, Eagle = -1, Eagle2=-1,stopwatch = -1, player = -1,player2=-1;

}
int TIME = 0;
bool /* dontPush = false, */ isAnim = false,isAnim2=false, deathSound = false,deathSound2=false;
int onLog = 0;
int onLog2 = 0; // onLog is for player1, onLog2 is for player2
std::vector<Line> line;

Player player,player2;

struct Eagle_
{

    const int speed_ms = 1000, fps = 60;
    double px, py = HEIGHT + CELL * 4;
};
void EagleSpawn();
void EagleSpawn2();
Eagle_ eagle,eagle2;
namespace Spawn
{
    void street(int line_i);
    void field(int line_i);
    void water(int line_i);
    void all(int line_i, bool isFirstLine);
}

void iDraw();
void motion();
void stopwatch();
bool collision(int line_i);
namespace Draw
{

    void street(int i, bool bg_only);
    void water(int i, bool bg_only);
    void field(int i);
    struct rgb
    {
        int r, g, b;
    };
    rgb water_bg, field_bg, street_bg, log, log_top;

}
namespace Horizontal
{
    int H = 0; 
    int scrollpx_dir;
    void scroll(int x);
    void scrollpx(); 
}

struct greater
{
    bool operator()(const Bar &a, const int &b) const
    {
        return a.pos > b;
    }
    bool operator()(const int &a, const Bar &b) const
    {
        return a > b.pos;
    }
};
struct less
{
    bool operator()(const Bar &a, const int &b) const
    {
        return a.pos < b;
    }
    bool operator()(const int &a, const Bar &b) const
    {
        return a < b.pos;
    }
};

void drawCoordinateAxes(double gap = CELL, int r = 0, int g = 0, int b = 0)
{
    iSetColor(r, g, b);

    for (int i = 0; i < WIDTH; i += gap)
    {
        iLine(i, 0, i, HEIGHT);

        if (i % (int)gap == 0)
        {
            char *str = (char *)malloc(10);
            sprintf(str, "%d", i);
            iText(i + 5, 5, str);
            free(str);
        }
    }

    for (int i = 0; i < HEIGHT; i += gap)
    {
        iLine(0, i, WIDTH, i);

        if (i % (int)gap == 0)
        {
            char *str = (char *)malloc(10);
            sprintf(str, "%d", i);
            iText(5, i + 5, str);
            free(str);
        }
    }
}

std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> resources; // images,audios
void load_resources()
{

    resources.push_back({{"assets\\images\\truck1.png", "assets\\images\\truck2.png", "assets\\images\\car1.png", "assets\\images\\car2.png",
                          "assets/images/rock.png", "assets/images/up.png", "assets/images/down.png", "assets/images/left.png", "assets/images/right.png", "assets/images/Dead.png", "assets/images/train(main).png", "assets/images/eagle.png", "assets/images/lilypad.png", "assets\\images\\tree1.png", "assets\\images\\tree2.png", "assets\\images\\tree3.png", "assets\\images\\tree4.png","assets/images/up2.png", "assets/images/down2.png", "assets/images/left2.png", "assets/images/right2.png", "assets/images/dead2.png"},
                         {"assets\\sounds\\cluck0.wav", "assets\\sounds\\cluck1.wav", "assets\\sounds\\death.wav",
                          "assets\\sounds\\traffic.ogg", "assets\\sounds\\eagle.wav", "assets\\sounds\\train.wav", "assets\\sounds\\train05x.wav", "assets\\sounds\\flown.wav", "assets\\sounds\\drown.wav", "assets\\sounds\\click.wav",
                        "assets/sounds/quack.wav"}});
    resources.push_back({{"assets\\images\\truck1.png", "assets\\images\\truck2.png", "assets\\images\\car1.png", "assets\\images\\car2.png",
                          "assets/images/rock.png", "assets/images/up3.png", "assets/images/down3.png", "assets/images/left3.png", "assets/images/right3.png", "assets/images/Dead3.png", "assets/images/train(main).png", "assets/images/eagle.png", "assets/images/lilypad.png", "assets\\images\\tree21.png", "assets\\images\\tree22.png", "assets\\images\\tree23.png", "assets\\images\\tree24.png","assets/images/up2.png", "assets/images/down2.png", "assets/images/left2.png", "assets/images/right2.png", "assets/images/dead2.png"},
                         {"assets\\sounds\\spring1.wav", "assets\\sounds\\spring2.wav", "assets\\sounds\\car_crash.wav",
                          "assets\\sounds\\traffic.ogg", "assets\\sounds\\eagle.wav", "assets\\sounds\\train.wav", "assets\\sounds\\train05x.wav", "assets\\sounds\\flown.wav", "assets\\sounds\\drown.wav", "assets\\sounds\\click.wav",
                        "assets/sounds/quack.wav"}});
                        
}
std::vector<Image *> Images = {
    &TRUCK1, &TRUCK2, &CAR1, &CAR2, &ROCK, &player.file[Up], &player.file[Down], &player.file[Left], &player.file[Right], &player.file[Dead], &TRAIN, &EAGLE, &LILYPAD, &TREE[0], &TREE[1], &TREE[2], &TREE[3], 
&player2.file[Up], &player2.file[Down], &player2.file[Left], &player2.file[Right], &player2.file[Dead]
};

std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
template <typename Iterator>
void shuffle_(Iterator start, Iterator ends)
{
    std::shuffle(start, ends, rng);
}
int ranint(int l, int r)
{
    std::uniform_int_distribution<int> dist(l, r);
    return dist(rng);
}
#ifdef _WIN32
#include <io.h>
#endif
void Load_Image()
{
    int i = 0;
    
    for (auto &image : Images)
    {
        
        iLoadImage(image, resources[resource_id].first[i].c_str());
        i++;
    }
    
    if (resource_id == 0)
    {
        Draw::water_bg = {99, 227, 255};

        Draw::log = {79, 47, 49};
        Draw::log_top = {141, 81, 77};
        Draw::field_bg = {143, 185, 73};

        auto count_files = [](const std::string &folder) // not for linux
        {
            int count = 0;
            struct _finddata_t fileinfo;
            intptr_t hFile = _findfirst((folder + "\\*").c_str(), &fileinfo);
            if (hFile != -1)
            {
                do
                {
                    if (!(fileinfo.attrib & _A_SUBDIR))
                        ++count;
                } while (_findnext(hFile, &fileinfo) == 0);
                _findclose(hFile);
            }
            return count;
        };

        splash.frames.resize(count_files("assets\\images\\Splash"));
        splash2.frames.resize(count_files("assets\\images\\Splash"));
        for (int i = 0; i < splash.frames.size(); ++i)
        {
            if(!splash.frames[i]) 
            splash.frames[i] = new Image;
            iLoadImage(splash.frames[i], std::string("assets\\images\\Splash\\" + std::to_string(i) + ".png").c_str());
            if(!splash2.frames[i]) 
            splash2.frames[i] = new Image;
            iLoadImage(splash2.frames[i], std::string("assets\\images\\Splash\\" + std::to_string(i) + ".png").c_str());
        
        }
    }
    if (resource_id == 1)
    {
        Draw::water_bg = {99, 227, 255};

        Draw::log = {79, 47, 49};
        Draw::log_top = {141, 81, 77};
        Draw::field_bg = {220, 156, 39};

        auto count_files = [](const std::string &folder) // not for linux
        {
            int count = 0;
            struct _finddata_t fileinfo;
            intptr_t hFile = _findfirst((folder + "\\*").c_str(), &fileinfo);
            if (hFile != -1)
            {
                do
                {
                    if (!(fileinfo.attrib & _A_SUBDIR))
                        ++count;
                } while (_findnext(hFile, &fileinfo) == 0);
                _findclose(hFile);
            }
            return count;
        };

        splash.frames.resize(count_files("assets\\images\\Splash"));
        splash2.frames.resize(count_files("assets\\images\\Splash"));
        for (int i = 0; i < splash.frames.size(); ++i)
        {
            if(!splash.frames[i]) 
            splash.frames[i] = new Image;
            iLoadImage(splash.frames[i], std::string("assets\\images\\Splash\\" + std::to_string(i) + ".png").c_str());
            if(!splash2.frames[i]) 
            splash2.frames[i] = new Image;
            iLoadImage(splash2.frames[i], std::string("assets\\images\\Splash\\" + std::to_string(i) + ".png").c_str());
        
        }
    }
    //loaded_once = true;
}
#ifdef _WIN32
#include <windows.h>
void DisableResize() {
    HWND hwnd = FindWindowA(NULL, "Crossy Road Lite"); // Use your window title
    if (hwnd) {
        LONG style = GetWindowLong(hwnd, GWL_STYLE);
        style &= ~(WS_MAXIMIZEBOX | WS_THICKFRAME); // Disable maximize + resizing
        SetWindowLong(hwnd, GWL_STYLE, style);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}
#endif

