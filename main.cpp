/**
* Author: Matthew Ma
* Assignment: Rise of the AI
* Date due: 2024-03-23, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f
#define PLATFORM_COUNT 18
#define ENEMY_COUNT 3

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_mixer.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"

// ––––– STRUCTS AND ENUMS ––––– //
struct GameState
{
    Entity* player;
    Entity* platforms;
    Entity* enemies;

    Mix_Music* bgm;
    Mix_Chunk* jump_sfx;
};

// ––––– CONSTANTS ––––– //
const int   WINDOW_WIDTH = 640,
            WINDOW_HEIGHT = 480;

const float BG_RED = 0.1922f,
            BG_BLUE = 0.549f,
            BG_GREEN = 0.9059f,
            BG_OPACITY = 1.0f;

const int   VIEWPORT_X = 0,
            VIEWPORT_Y = 0,
            VIEWPORT_WIDTH = WINDOW_WIDTH,
            VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char  V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
            F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND  = 1000.0;
const char  SPRITESHEET_FILEPATH[]  = "assets/goku.png",
            PLATFORM_FILEPATH[]     = "assets/wood.png",
            ENEMY_FILEPATH[]        = "assets/dragonball.png",
            FONT_FILEPATH[] = "assets/font1.png";


const char  BGM_FILEPATH[]          = "assets/audio/wind.mp3",
            BOUNCING_SFX_FILEPATH[] = "assets/audio/ambience.wav";
//I can't hear audio but it is not listed as a req
const int   LOOP_FOREVER     = 0;  // -1 means loop forever in Mix_PlayMusic; 0 means play once and loop zero times

const int NUMBER_OF_TEXTURES = 1;
const GLint LEVEL_OF_DETAIL  = 0;
const GLint TEXTURE_BORDER   = 0;

// BGM
const int   CD_QUAL_FREQ    = 44100,  // CD quality
            AUDIO_CHAN_AMT  = 2,      // Stereo
            AUDIO_BUFF_SIZE = 4096;

// SFX
const int   PLAY_ONCE   = 0,
            NEXT_CHNL   = -1,  // next available channel
            MUTE_VOL    = 0,
            MILS_IN_SEC = 1000,
            ALL_SFX_CHN = -1;

const int FONTBANK_SIZE = 16;

bool win = false;
bool lose = false;

int enemy_slain = 0;

// ––––– GLOBAL VARIABLES ––––– //
GameState g_game_state;

SDL_Window* g_display_window;
bool g_game_is_running = true;

ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float g_previous_ticks = 0.0f;
float g_time_accumulator = 0.0f;

// Audio
Mix_Music* g_music;
Mix_Chunk* g_bouncing_sfx;

// ———— GENERAL FUNCTIONS ———— //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint textureID;
    glGenTextures(NUMBER_OF_TEXTURES, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return textureID;
}


void initialise()
{
    // Initialising both the video AND audio subsystems
    // We did something similar when we talked about video game controllers
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    g_display_window = SDL_CreateWindow("Hello, Audio!",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif
    // ––––– VIDEO SETUP ––––– //
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

  

    // ––––– PLATFORMS ––––– //
    GLuint platform_texture_id = load_texture(PLATFORM_FILEPATH);

    g_game_state.platforms = new Entity[PLATFORM_COUNT];

    g_game_state.platforms[PLATFORM_COUNT - 1].m_texture_id = platform_texture_id;
    g_game_state.platforms[PLATFORM_COUNT - 1].set_position(glm::vec3(-5.0f, -2.35f, 0.0f));
    g_game_state.platforms[PLATFORM_COUNT - 1].set_width(0.4f);
    g_game_state.platforms[PLATFORM_COUNT - 1].update(0.0f, NULL, NULL, 0);

    for (int i = 0; i < PLATFORM_COUNT - 2; i++)
    {
        g_game_state.platforms[i].m_texture_id = platform_texture_id;
        g_game_state.platforms[i].set_position(glm::vec3(i-5.0f, -3.0f, 0.0f));
        g_game_state.platforms[i].set_width(0.4f);
        g_game_state.platforms[i].update(0.0f, NULL, NULL, 0);
    }

    g_game_state.platforms[PLATFORM_COUNT - 2].m_texture_id = platform_texture_id;
    g_game_state.platforms[PLATFORM_COUNT - 2].set_position(glm::vec3(5.0f, -2.5f, 0.0f));
    g_game_state.platforms[PLATFORM_COUNT - 2].set_width(0.4f);
    g_game_state.platforms[PLATFORM_COUNT - 2].update(0.0f, NULL, NULL, 0);

    g_game_state.platforms[PLATFORM_COUNT - 3].m_texture_id = platform_texture_id;
    g_game_state.platforms[PLATFORM_COUNT - 3].set_position(glm::vec3(0.0f, -0.0f, 0.0f));
    g_game_state.platforms[PLATFORM_COUNT - 3].set_width(0.4f);
    g_game_state.platforms[PLATFORM_COUNT - 3].update(0.0f, NULL, NULL, 0);

    g_game_state.platforms[PLATFORM_COUNT - 4].m_texture_id = platform_texture_id;
    g_game_state.platforms[PLATFORM_COUNT - 4].set_position(glm::vec3(1.0f, -0.0f, 0.0f));
    g_game_state.platforms[PLATFORM_COUNT - 4].set_width(0.4f);
    g_game_state.platforms[PLATFORM_COUNT - 4].update(0.0f, NULL, NULL, 0);

    g_game_state.platforms[PLATFORM_COUNT - 5].m_texture_id = platform_texture_id;
    g_game_state.platforms[PLATFORM_COUNT - 5].set_position(glm::vec3(2.0f, -2.0f, 0.0f));
    g_game_state.platforms[PLATFORM_COUNT - 5].set_width(0.4f);
    g_game_state.platforms[PLATFORM_COUNT - 5].update(0.0f, NULL, NULL, 0);

    g_game_state.platforms[PLATFORM_COUNT - 6].m_texture_id = platform_texture_id;
    g_game_state.platforms[PLATFORM_COUNT - 6].set_position(glm::vec3(3.0f, -1.0f, 0.0f));
    g_game_state.platforms[PLATFORM_COUNT - 6].set_width(0.4f);
    g_game_state.platforms[PLATFORM_COUNT - 6].update(0.0f, NULL, NULL, 0);

    g_game_state.platforms[PLATFORM_COUNT - 7].m_texture_id = platform_texture_id;
    g_game_state.platforms[PLATFORM_COUNT - 7].set_position(glm::vec3(-2.0f, -1.0f, 0.0f));
    g_game_state.platforms[PLATFORM_COUNT - 7].set_width(0.4f);
    g_game_state.platforms[PLATFORM_COUNT - 7].update(0.0f, NULL, NULL, 0);

    g_game_state.platforms[PLATFORM_COUNT - 8].m_texture_id = platform_texture_id;
    g_game_state.platforms[PLATFORM_COUNT - 8].set_position(glm::vec3(-2.0f, -1.0f, 0.0f));
    g_game_state.platforms[PLATFORM_COUNT - 8].set_width(0.4f);
    g_game_state.platforms[PLATFORM_COUNT - 8].update(0.0f, NULL, NULL, 0);

    // ––––– PLAYER (GEORGE) ––––– //
    // Existing
    g_game_state.player = new Entity();
    g_game_state.player->set_entity_type(PLAYER);
    g_game_state.player->set_position(glm::vec3(2.0f, 2.0f, 0.0f));
    g_game_state.player->set_movement(glm::vec3(0.0f));
    g_game_state.player->set_speed(1.0f);
    g_game_state.player->set_acceleration(glm::vec3(0.0f, -4.905f, 0.0f));
    g_game_state.player->m_texture_id = load_texture(SPRITESHEET_FILEPATH);

    // Walking
    // Walking
    g_game_state.player->m_walking[g_game_state.player->LEFT] = new int[3] { 3, 4, 5 };
    g_game_state.player->m_walking[g_game_state.player->RIGHT] = new int[3] { 6, 7, 8 };
    g_game_state.player->m_walking[g_game_state.player->UP] = new int[3] { 9, 10, 11 };
    g_game_state.player->m_walking[g_game_state.player->DOWN] = new int[3] { 0, 1, 2 };

    g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];  // start George looking right
    g_game_state.player->m_animation_frames = 3;
    g_game_state.player->m_animation_index = 0;
    g_game_state.player->m_animation_time = 0.0f;
    g_game_state.player->m_animation_cols = 3;
    g_game_state.player->m_animation_rows = 4;
    g_game_state.player->set_height(0.9f);
    g_game_state.player->set_width(0.9f);

    // Jumping
    g_game_state.player->set_jumping_power(4.0f);

    // ––––– ENEMY (SOPHIE) ––––– //
    GLuint enemy_texture_id = load_texture(ENEMY_FILEPATH);

    g_game_state.enemies = new Entity[ENEMY_COUNT];
    g_game_state.enemies[0].set_entity_type(ENEMY);
    g_game_state.enemies[0].set_ai_type(RUNNER);
    g_game_state.enemies[0].set_ai_state(IDLE);
    g_game_state.enemies[0].m_texture_id = enemy_texture_id;
    g_game_state.enemies[0].set_position(glm::vec3(1.5f, 0.0f, 0.0f));
    g_game_state.enemies[0].set_movement(glm::vec3(0.0f));
    g_game_state.enemies[0].set_speed(0.5f);
    g_game_state.enemies[0].set_acceleration(glm::vec3(0.0f, -9.81f, 0.0f));

    g_game_state.enemies[1].set_entity_type(ENEMY);
    g_game_state.enemies[1].set_ai_type(JUMPER);
    g_game_state.enemies[1].set_jumping_power(3.0f);
    g_game_state.enemies[1].set_ai_state(IDLE);
    g_game_state.enemies[1].m_texture_id = enemy_texture_id;
    g_game_state.enemies[1].set_position(glm::vec3(3.0f, 0.0f, 0.0f));
    g_game_state.enemies[1].set_movement(glm::vec3(0.0f));
    g_game_state.enemies[1].set_speed(0.5f);
    g_game_state.enemies[1].set_acceleration(glm::vec3(0.0f, -1.5f, 0.0f));

    g_game_state.enemies[2].set_entity_type(ENEMY);
    g_game_state.enemies[2].set_ai_type(PATROLLER);
    g_game_state.enemies[2].set_ai_state(PATROL);
    g_game_state.enemies[2].m_texture_id = enemy_texture_id;
    g_game_state.enemies[2].set_position(glm::vec3(-4.8f, 0.0f, 0.0f));
    g_game_state.enemies[2].set_movement(glm::vec3(1.0f));
    g_game_state.enemies[2].set_speed(0.5f);
    g_game_state.enemies[2].set_acceleration(glm::vec3(0.0f, -9.81f, 0.0f));

    // ––––– AUDIO STUFF ––––– //
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

    g_game_state.bgm = Mix_LoadMUS(BGM_FILEPATH);
    Mix_PlayMusic(g_game_state.bgm, -1);
    Mix_VolumeMusic(MIX_MAX_VOLUME / 4.0f);

    g_game_state.jump_sfx = Mix_LoadWAV(BOUNCING_SFX_FILEPATH);

    // ––––– GENERAL ––––– //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    g_game_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            // End game
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running = false;
                break;

            case SDLK_SPACE:
                // Jump
                if (g_game_state.player->m_collided_bottom)
                {
                    g_game_state.player->m_is_jumping = true;
                    Mix_PlayChannel(
                        NEXT_CHNL,       // using the first channel that is not currently in use...
                        g_bouncing_sfx,  // ...play this chunk of audio...
                        PLAY_ONCE        // ...once.
                    );
                }
                break;

            case SDLK_m:
                // Mute volume
                Mix_HaltMusic();
                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);

    if (!lose) {
        if (key_state[SDL_SCANCODE_LEFT])
        {
            g_game_state.player->move_left();
            g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->LEFT];
        }
        else if (key_state[SDL_SCANCODE_RIGHT])
        {
            g_game_state.player->move_right();
            g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];
        }

        // This makes sure that the player can't move faster diagonally
        if (glm::length(g_game_state.player->get_movement()) > 1.0f)
        {
            g_game_state.player->set_movement(glm::normalize(g_game_state.player->get_movement()));
        }
    }
}

void update()
{
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_time_accumulator;

    if (delta_time < FIXED_TIMESTEP)
    {
        g_time_accumulator = delta_time;
        return;
    }

    while (delta_time >= FIXED_TIMESTEP) {
        Entity* collidables = new Entity[PLATFORM_COUNT + ENEMY_COUNT];

        for (int i = 0; i < PLATFORM_COUNT; i++) {
            collidables[i] = g_game_state.platforms[i];
        }

        for (int i = 0; i < ENEMY_COUNT; i++) {
            collidables[PLATFORM_COUNT + i] = g_game_state.enemies[i];
        }

        g_game_state.player->update(FIXED_TIMESTEP, g_game_state.player, collidables, PLATFORM_COUNT + ENEMY_COUNT);

        for (int i = 0; i < ENEMY_COUNT; i++) g_game_state.enemies[i].update(FIXED_TIMESTEP, g_game_state.player, g_game_state.platforms, PLATFORM_COUNT);

        delta_time -= FIXED_TIMESTEP;
    }

    Entity* collided = g_game_state.player->collided;
    if (collided != nullptr) {
        if (collided->get_entity_type() == ENEMY && (g_game_state.player->left_enemy or g_game_state.player->right_enemy or g_game_state.player->top_enemy))
        {
            lose = true;
        }
        if (collided->get_entity_type() == ENEMY && g_game_state.player->bottom_enemy)
        {
            AIType type = g_game_state.player->collided->get_ai_type();
            for (int i = 0; i < ENEMY_COUNT; i++) {
                if (type == g_game_state.enemies[i].get_ai_type()) {
                    g_game_state.enemies[i].deactivate();
                    enemy_slain += 1;
                }
            }
        }
    }

    if (enemy_slain == ENEMY_COUNT && !lose) {
        win = true;
    }


    g_time_accumulator = delta_time;
}

void draw_text(ShaderProgram* program, GLuint font_texture_id, std::string text, float screen_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for each character
    // Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their position
        //    relative to the whole sentence)
        int spritesheet_index = (int)text[i];  // ascii value of character
        float offset = (screen_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float)(spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float)(spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * screen_size), 0.5f * screen_size,
            offset + (-0.5f * screen_size), -0.5f * screen_size,
            offset + (0.5f * screen_size), 0.5f * screen_size,
            offset + (0.5f * screen_size), -0.5f * screen_size,
            offset + (0.5f * screen_size), 0.5f * screen_size,
            offset + (-0.5f * screen_size), -0.5f * screen_size,
            });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
            });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    program->set_model_matrix(model_matrix);
    glUseProgram(program->get_program_id());

    glVertexAttribPointer(program->get_position_attribute(), 2, GL_FLOAT, false, 0, vertices.data());
    glEnableVertexAttribArray(program->get_position_attribute());
    glVertexAttribPointer(program->get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0, texture_coordinates.data());
    glEnableVertexAttribArray(program->get_tex_coordinate_attribute());

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int)(text.size() * 6));

    glDisableVertexAttribArray(program->get_position_attribute());
    glDisableVertexAttribArray(program->get_tex_coordinate_attribute());
}

void render()
{
    glClear(GL_COLOR_BUFFER_BIT);

    g_game_state.player->render(&g_shader_program);

    for (int i = 0; i < PLATFORM_COUNT; i++) g_game_state.platforms[i].render(&g_shader_program);
    for (int i = 0; i < ENEMY_COUNT; i++)    g_game_state.enemies[i].render(&g_shader_program);

    GLuint g_font_id = load_texture(FONT_FILEPATH);

    if (win and !lose)
    {
        draw_text(&g_shader_program, g_font_id, "You Win!", 0.4, 0.01f, glm::vec3(-3.0f, 0.0f, 0));
    }

    if (lose and !win)
    {
        draw_text(&g_shader_program, g_font_id, "You Lose!", 0.4, 0.01f, glm::vec3(-3.0f, 0.0f, 0));
    }

    SDL_GL_SwapWindow(g_display_window);
}

void shutdown()
{
    SDL_Quit();

    delete[] g_game_state.platforms;
    delete   g_game_state.player;
}

// ––––– GAME LOOP ––––– //
int main(int argc, char* argv[])
{
    initialise();

    while (g_game_is_running)
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}
