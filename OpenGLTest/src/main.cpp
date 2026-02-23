#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <filesystem> 
#include <iostream>

#include "Game.h"

int main(int argc, char* argv[])
{
    // -------------------------
    // SDL Audio 初期化
    // -------------------------
    if (SDL_Init(SDL_INIT_AUDIO) != 0)
    {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0)
    {
        std::cerr << "Mix_OpenAudio Error: " << Mix_GetError() << std::endl;
        return -1;
    }

    // -------------------------
    // BGM 読み込み
    // -------------------------
    // ここで現在の作業ディレクトリを表示
    std::cout << "cwd: "
        << std::filesystem::current_path().string()
        << std::endl;

    Mix_Music* bgm = Mix_LoadMUS("assets/bgm.ogg");
    if (!bgm)
    {
        std::cerr << "Mix_LoadMUS Error: "
            << Mix_GetError()
            << std::endl;
    }
    else
    {
        Mix_PlayMusic(bgm, -1); // ループ再生
    }

    // -------------------------
    // 既存ゲーム起動
    // -------------------------
    Game game(800, 600, "OpenGL Test");
    game.Run();

    // -------------------------
    // 終了処理
    // -------------------------
    if (bgm)
        Mix_FreeMusic(bgm);

    Mix_CloseAudio();
    SDL_Quit();

    return 0;
}