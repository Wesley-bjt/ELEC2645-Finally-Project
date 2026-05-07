#include "Game_2.h"
#include "InputHandler.h"
#include "Menu.h"
#include "LCD.h"
#include "Buzzer.h"
#include "stm32l476xx.h"
#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdio.h>   
#include <sys/_intsup.h>
#include "Joystick.h"
#include "PWM.h" //LED
#include "Utils.h"  //AABB
#include <stdlib.h> //abs
//------------------------
extern ST7789V2_cfg_t cfg0;
extern Buzzer_cfg_t buzzer_cfg;  // Buzzer control
extern PWM_cfg_t pwm_cfg;      // LED PWM control
extern Joystick_cfg_t joystick_cfg;  // Joystick control
extern Joystick_t joystick_data;
//--------------------------------------------
// Frame rate for this game (in milliseconds) - runs slower than Game 1
#define GAME2_FRAME_TIME_MS 50  // ~20 FPS (different from Game 1!)           
//--------state------------------
#define STATE_INIT  0  // initialization stage
#define STATE_PLAY  1  // Game running stage
#define STATE_OVER  2  // Game over stage
static uint8_t current_state = STATE_INIT;

//-------Map material--------------
#define EMPTY 0
#define BRICK 1
#define RUBBER 2
#define IRON  3

//----Map resolution-----
#define COLUMN 24
#define ROW 20
#define SIZE 10

//----------- bullet shooting Variables------------
#define FIRE_DELAY 200
#define FIRE_DELAY_ENEMY 800
#define BULLET_MAX_TIME 2000
#define MAX_BULLET 50

//------------- enemy --------------------
#define MAX_ENEMY 5
//----Map --------------------------------
static uint8_t game_map[ROW][COLUMN];  // actually used in the game

const uint8_t map_template[ROW][COLUMN] ={    
    {10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10}, 
    {10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,10}, 
    {10, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 3, 0, 0, 0, 0,10}, 
    {10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0,10}, 
    {10, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0,10}, 
    {10, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0,10},
    {10, 0, 0, 0, 5, 0, 0, 0, 4, 0, 0, 0, 0, 2, 0, 0, 0, 0, 6, 6, 6, 6, 6,10},
    {10, 0, 0, 0, 5, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0,10}, 
    {10, 0, 0, 0, 5, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0,10},
    {10, 0, 0, 0, 5, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0,10}, 
    {10, 0, 0, 0, 5, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,10},
    {10, 5, 5, 5, 5, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,10}, 
    {10, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 8, 8, 8, 8, 0, 0, 9, 0, 0, 0,10}, 
    {10, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 9, 0, 0, 0,10},
    {10, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 9, 0, 0, 0,10}, 
    {10, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 9, 0, 0, 0,10},
    {10, 1, 1, 1, 1, 1, 0, 0, 4, 0, 0, 0, 0, 8, 8, 8, 8, 0, 0, 9, 9, 9, 9,10}, 
    {10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0,10},
    {10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0,10},
    {10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10}  
};

const uint8_t map_direction[ROW][COLUMN] = {
    {13,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,13}, 
    {12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,12}, 
    {13,11,11,11,11,13, 0, 0, 0, 0, 0,13,11,13,11,13, 0, 0,13, 0, 0, 0, 0,12}, 
    {12,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,12, 0, 0, 0, 0,12, 0, 0, 0,0,12}, 
    {12,0, 0, 0, 0, 0, 0, 0,13, 0, 0, 0, 0,12, 0, 0, 0, 0,12, 0, 0, 0,0,12}, 
    {12,0, 0, 0, 0, 0, 0, 0,12, 0, 0, 0, 0,12, 0, 0, 0, 0,12, 0, 0, 0,0,12},
    {12,0, 0, 0, 13, 0, 0, 0,12, 0, 0, 0, 0,13, 0, 0, 0, 0,13, 11, 11, 11,11,13},
    {12,0, 0, 0,12, 0, 0, 0,12, 0, 0, 0, 0, 0, 0, 0, 0, 0,12, 0, 0, 0,0,12}, 
    {12,0, 0, 0,12, 0, 0, 0,12, 0, 0, 0, 0, 0, 0, 0, 0, 0,12, 0, 0, 0,0,12},
    {12,0, 0, 0,12, 0, 0, 0,13,11,11,11,11,11,11,11,11,11,13, 0, 0, 0,0,12}, 
    {12,0, 0, 0,12, 0, 0, 0,12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0,12},
    {13,11, 11, 11,13,0, 0, 0,12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0,12}, 
    {12,0, 0, 0, 0, 0, 0, 0,12, 0, 0, 0, 0,13,11,11,13, 0, 0,13, 0, 0,0,12}, 
    {12,0, 0, 0, 0, 0, 0, 0,12, 0, 0, 0, 0,12, 0, 0, 0, 0, 0,12, 0, 0,0,12},
    {12,0, 0, 0, 0, 0, 0, 0,12, 0, 0, 0, 0,12, 0, 0, 0, 0, 0,12, 0, 0,0,12}, 
    {12,0, 0, 0, 0, 0, 0, 0,12, 0, 0, 0, 0,12, 0, 0, 0, 0, 0,12, 0, 0,0,12},
    {13,11,11,11,11,13, 0, 0,13, 0, 0, 0, 0,13,11,11,13, 0, 0,13,11,11,11,13}, 
    {12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,12, 0, 0, 0, 0, 0, 0, 0, 0, 0,12},
    {12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,12, 0, 0, 0, 0, 0, 0, 0, 0, 0,12},
    {13,11,11,11,11,11,11,11,11,11,11,11,11,13,11,11,11,11,11,11,11,11,11,13}  
};
//---------------------------------------------------------------------------------------------------------------

//------Use structures to store tank-----------
typedef struct {
    int16_t x;        
    int16_t y;        
    int16_t size;     
    int16_t speed; 
    uint32_t color;
    Direction dir;
    int16_t life;
    uint8_t active; 
    uint32_t shoot_time;
} Tank_me;

static Tank_me player_tank;
static Tank_me Enemy_tank[MAX_ENEMY];
//----------Store the path of enemy tanks----------
const int16_t enemy_end_x[MAX_ENEMY] = {2 * SIZE,6 * SIZE,16 * SIZE,20 * SIZE, 14 * SIZE};
const int16_t enemy_end_y[MAX_ENEMY] = {9 * SIZE + 40 , 17 * SIZE + 40,7 * SIZE + 40 ,7 * SIZE +40,17 * SIZE + 40};
const int16_t enemy_start_x[MAX_ENEMY] = {2 * SIZE,2 * SIZE,9 * SIZE,20 * SIZE,21 * SIZE};
const int16_t enemy_start_y[MAX_ENEMY] = {3 * SIZE + 40, 17 * SIZE + 40,7 * SIZE + 40,14 * SIZE + 40,17 * SIZE + 40};
const Direction enemy_start_dir[MAX_ENEMY] = {S, E, E, N, W};
//---------Store bullet-----------
typedef struct {
    int16_t x;        
    int16_t y;        
    int16_t radius;     
    int16_t speed; 
    uint32_t color;
    Direction dir;  
    uint8_t active;
    uint32_t fire_time;
} Bullet_t;

static Bullet_t player_bullets[MAX_BULLET];
static Bullet_t enemy_bullets[MAX_BULLET];
//----function prototype-------
//--------- Map -----------------------------------
void Map_Generate(void);
void Map_Draw(void);
void HUD_Draw(Tank_me *player, uint32_t player_score);
//--------------------Tank--------------------------------
void Tank_player_init(Tank_me * tank);
void Tank_player_update(Tank_me * tank, Direction joy_dir, Tank_me * enemy);
void Tank_player_draw(Tank_me * tank);
void Tank_enemy_init(Tank_me * tank);
void Tank_enemy_AI(Tank_me * enemy, Tank_me * tank);
void Tank_enemy_draw(Tank_me * enemy);
//--------------- collision -------------------------------------
uint8_t Check_Collision(int16_t location_x, int16_t location_y, int16_t size);
uint8_t Check_player_enemy(int16_t next_x, int16_t next_y, int16_t size, Tank_me *enemy);
uint8_t Bullet_Tank_Check(Bullet_t *bullet, Tank_me *tank);
uint8_t Bullet_Wall_Check(int16_t location_x, int16_t location_y, Bullet_t *bullet,uint8_t *wall_type);
void Player_Hit_Enemy_Check(Bullet_t *player_bullet, Tank_me *enemy, uint32_t *player_score);
void Enemy_Hit_Player_Check(Bullet_t *enemy_bullet, Tank_me *player);
//---------bullet-------------------------------------------------
void Bullet_player_init(Bullet_t * Bullet,Tank_me * tank);
void Bullet_enemy_init(Bullet_t * Bullet,Tank_me * enemy);
void Bullet_update(Bullet_t * Bullet);
void Bullet_draw(Bullet_t * Bullet);
void Bullet_Rubber(Bullet_t *bullet, uint8_t wall_type);
void Bullet_Brick(Bullet_t *bullet);
void Bullet_Iron(Bullet_t *bullet);
//----------------------- LED ------------------------
void led_alarm(Tank_me * player,uint32_t *led_alarm, uint8_t *led_state);


MenuState Game2_Run(void) {
    uint32_t player_score = 0;
    uint32_t alarm_time = 0;
    uint8_t lightstate = 0;
    MenuState exit_state = MENU_STATE_HOME; 
    current_state = STATE_INIT;  //initialize every time when enter the game
    // Game's own loop - runs until exit condition
    while (1) {
        uint32_t frame_start = HAL_GetTick();
        Input_Read();
        if (current_input.btn2_pressed) {
            //=================== Reset all states =================================
            player_score = 0;
            for(uint16_t i = 0; i < MAX_BULLET; i++) {
                player_bullets[i].active = 0;
                enemy_bullets[i].active = 0;
            }
            //======================================================================
            exit_state = MENU_STATE_HOME;
            break; 
        }
        Joystick_Read(&joystick_cfg, &joystick_data);
        Direction current_direction = joystick_data.direction;

        switch(current_state){

            case STATE_INIT:
            LCD_clear();                   
            LCD_Fill_Buffer(0); 
            Map_Generate();
            Tank_player_init(&player_tank);
            Tank_enemy_init(Enemy_tank);
            LCD_Refresh(&cfg0);
            current_state = STATE_PLAY;
            break;

            case STATE_PLAY:
            //==================== Update the data =============================================
            Tank_player_update(&player_tank, current_direction,Enemy_tank);
            Tank_enemy_AI(Enemy_tank, &player_tank);
            Bullet_update(enemy_bullets);
            Bullet_player_init(player_bullets,&player_tank);
            Bullet_update(player_bullets);
            Player_Hit_Enemy_Check(player_bullets, Enemy_tank,&player_score);
            Enemy_Hit_Player_Check(enemy_bullets, &player_tank);
            led_alarm(&player_tank, &alarm_time, &lightstate);
            //================= Enemy generate ============================
            uint8_t survive = 0;
            for(uint16_t i = 0; i < MAX_ENEMY; i++){
                if(Enemy_tank[i].active == 1){
                    survive = survive + 1;
                    Bullet_enemy_init(enemy_bullets, &Enemy_tank[i]);
                }
            }
            if(survive == 0){
                //======== Generate new map and reset positions =================
                Tank_enemy_init(Enemy_tank);
                Map_Generate();
                if (Check_Collision(player_tank.x, player_tank.y, player_tank.size) == 1) {
                    player_tank.x = 1 * SIZE;
                    player_tank.y = 1 * SIZE + 40;
                }
            }
            
            //================== Draw the game ========================================
            LCD_Fill_Buffer(0); 
            Tank_player_draw(&player_tank); 
            Tank_enemy_draw(Enemy_tank); 
            Bullet_draw(enemy_bullets);
            Bullet_draw(player_bullets);
            Map_Draw();
            HUD_Draw(&player_tank,player_score);
            LCD_Refresh(&cfg0);
            break;
            
            case STATE_OVER:
            //==================== Output the game result ============================
            LCD_Fill_Buffer(0); 
            LCD_printString("GAME OVER!", 40, 80, 1, 3);
            char score[20];
            sprintf(score, "Score: %lu", player_score);
            LCD_printString(score, 65, 120, 1, 2);
            LCD_printString("Press BT2 to exit", 25, 160, 1, 2);
            LCD_Refresh(&cfg0);
            break;
            default:
            exit_state = MENU_STATE_HOME;  // Default: return to menu
            break;
        }
        // Frame timing - wait for remainder of frame time
        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < GAME2_FRAME_TIME_MS) {
        HAL_Delay(GAME2_FRAME_TIME_MS - frame_time);
        }
    }
    return exit_state;
}

//================= Map =====================================

void Map_Generate(void){
    uint8_t id_array[10];
    for (uint8_t i = 1; i <= 9; i++) {
        id_array[i] = Random_U16(3) + 1; // Generate random materials
    }
    for (uint8_t x = 0; x < ROW; x++) {
        for (uint8_t y = 0; y < COLUMN; y++) {
            uint8_t id = map_template[x][y];
            if(id <= 9 && id > 0){
                game_map[x][y] = id_array[id];
            }
            else if (id == 10) {
                game_map[x][y] = IRON; // Edge walls are Iron
            }
            else{
                game_map[x][y] = EMPTY;
            }
        }
    }
}


void Map_Draw(void) {
    uint8_t thickness = 2;
    uint8_t offset = (SIZE-thickness)/2;
    for (uint8_t x = 0; x < ROW; x++) {
        for (uint8_t y = 0; y < COLUMN; y++) {
            uint8_t material = game_map[x][y];
            if(material == EMPTY){
                continue;
            }
            uint16_t axis_x = y * SIZE;
            uint16_t axis_y = (x * SIZE) + 40; // offset for HUD
            uint16_t color;
            if (material == BRICK) {
                color = 13; 
            } 
            else if (material == IRON) {
                color = 12; 
            } 
            else if(material == RUBBER){ 
                color = 14; 
            }

            uint8_t direction = map_direction[x][y];
            if (direction == 11){
                LCD_Draw_Rect(axis_x, axis_y + offset , SIZE, thickness, color, 1);// Horizontal
            }
            else if (direction == 12){
                LCD_Draw_Rect(axis_x + offset, axis_y, thickness, SIZE, color, 1); // Vertical
            }
            else if (direction == 13){
                LCD_Draw_Rect(axis_x + offset, axis_y + offset, thickness, thickness, color, 1); // Connector
                if (x > 0 && map_direction[x-1][y] != EMPTY) {
                    LCD_Draw_Rect(axis_x + offset, axis_y, thickness, offset, color, 1);
                }
                if (x < ROW - 1 && map_direction[x+1][y] != EMPTY) {
                    LCD_Draw_Rect(axis_x + offset, axis_y + offset + thickness, thickness, offset, color, 1);
                }
                if (y > 0 && map_direction[x][y-1] != EMPTY) {
                    LCD_Draw_Rect(axis_x, axis_y + offset, offset, thickness, color, 1);
                }
                if (y < COLUMN - 1 && map_direction[x][y+1] != EMPTY) {
                    LCD_Draw_Rect(axis_x + offset + thickness, axis_y + offset, offset, thickness, color, 1);
                }
            }
        }
    }
}


void HUD_Draw(Tank_me *player, uint32_t player_score){
    char life[20];
    char score[20];
    sprintf(life, "Life:%d", player->life);
    sprintf(score, "Score:%lu", player_score);
    LCD_printString(life, 10, 12, 1, 2);   
    LCD_printString(score, 110, 12, 1, 2);
}

//==================== player tank ===================================
void Tank_player_init(Tank_me * tank){
    tank->x = 1*SIZE;  
    tank->y = 1*SIZE + 40; 
    tank->size = 10;
    tank->speed = 4;
    tank->dir = E;
    tank ->color = 2;
    tank-> life = 5;
    tank->active = 1;
    tank->shoot_time = 0;
}


void Tank_player_draw(Tank_me * tank){
    uint8_t gun_size = 4; 
    uint8_t gun_color = tank->color;
    LCD_Draw_Rect(tank->x, tank->y, tank->size, tank->size, tank->color, 1);
    uint8_t offset = (tank->size - gun_size) / 2;
    switch (tank->dir) {
        case N:  
            LCD_Draw_Rect(tank->x + offset, tank->y - gun_size, gun_size, gun_size, gun_color, 1);
            break;
        case E:
            LCD_Draw_Rect(tank->x + tank->size, tank->y + offset, gun_size, gun_size, gun_color, 1);
            break;
        case S:
            LCD_Draw_Rect(tank->x + offset, tank->y + tank->size, gun_size, gun_size, gun_color, 1);
            break;
        case W:
            LCD_Draw_Rect(tank->x - gun_size, tank->y + offset, gun_size, gun_size, gun_color, 1);
            break;
        case NW: 
            LCD_Draw_Rect(tank->x - gun_size, tank->y - gun_size, gun_size, gun_size, gun_color, 1); 
            break;
        case NE: 
            LCD_Draw_Rect(tank->x + tank->size , tank->y - gun_size, gun_size, gun_size, gun_color, 1); 
            break;
        case SW: 
            LCD_Draw_Rect(tank->x - gun_size, tank->y + tank->size, gun_size, gun_size, gun_color, 1); 
            break;
        case SE: 
            LCD_Draw_Rect(tank->x + tank->size, tank->y + tank->size, gun_size, gun_size, gun_color, 1); 
            break;
        default:
            break;
    }
}

void Tank_player_update(Tank_me * tank, Direction joy_dir, Tank_me * enemy) {
    if (joy_dir == CENTRE) {
        return; 
    }

    tank->dir = joy_dir; 
    
    int16_t next_x = tank->x;
    int16_t next_y = tank->y;

    if (joy_dir == N || joy_dir == NW || joy_dir == NE ){
        next_y -= tank->speed;
    }
    if (joy_dir == S || joy_dir == SW || joy_dir == SE ){
        next_y += tank->speed;
    }
    if (joy_dir == W || joy_dir == NW || joy_dir == SW ){
        next_x -= tank->speed;
    } 
    if (joy_dir == E || joy_dir == NE || joy_dir == SE ){
        next_x += tank->speed;
    } 
    //---------------- collision with enemies ---------------------
    int16_t hit_enemy_x = 0;
    int16_t hit_enemy_y = 0;
    for (int16_t i = 0; i < MAX_ENEMY; i++){
        if (enemy[i].active == 0){
            continue;
        } 
        if (Check_player_enemy(next_x, tank->y, tank->size, &enemy[i]) == 1){
            hit_enemy_x = 1;
        }
        if(Check_player_enemy(tank->x, next_y, tank->size, &enemy[i]) == 1){
            hit_enemy_y = 1;
        }
    }
    //------------ collision with walls --------------------------
    if (Check_Collision(next_x, tank->y, tank->size) == 0 && hit_enemy_x == 0) {
        tank->x = next_x; 
    }
    if (Check_Collision(tank->x, next_y, tank->size) == 0 && hit_enemy_y == 0) {
        tank->y = next_y;
    }
}

//=========================== collision detection ==============================================
uint8_t Check_Collision(int16_t location_x, int16_t location_y, int16_t size){
    //------------- Prevent overflow ------------------
    if(location_x < 0 || location_x + size > COLUMN * SIZE){
        return 1;
    }
    if(location_y < 40 || location_y + size > (ROW * SIZE) + 40){
        return 1;
    }
    //--------------------------------------------------------------
    int16_t west = location_x / SIZE;
    int16_t east = (location_x + size - 1) / SIZE;
    int16_t north = (location_y - 40) / SIZE;              
    int16_t south = (location_y + size - 40 - 1) / SIZE;
    for(int16_t y = north;y <= south;y++){
        for(int16_t x = west;x <= east;x++){
            if (game_map[y][x] != EMPTY) {
                return 1;
            }
        }
    }
    return 0;
}

uint8_t Check_player_enemy(int16_t next_x, int16_t next_y, int16_t size, Tank_me *enemy){
    AABB my_box = {next_x, next_y, size, size};
    AABB enemy_box = {enemy->x, enemy->y, enemy->size, enemy->size};
    if (AABB_Collides(&my_box, &enemy_box) == 1) {
        return 1;
    }
    return 0;
}

uint8_t Bullet_Wall_Check(int16_t location_x, int16_t location_y, Bullet_t *bullet,uint8_t *wall_type) {
    int16_t size = bullet->radius * 2;
    int16_t west = location_x / SIZE;
    int16_t east = (location_x + size - 1) / SIZE;
    int16_t north = (location_y - 40) / SIZE;              
    int16_t south = (location_y + size - 40 - 1) / SIZE;
    int16_t material = EMPTY;
    for(int16_t y = north; y <= south; y++) {
        for(int16_t x = west; x <= east; x++) {
            material = game_map[y][x];
            if (material == BRICK){
                game_map[y][x] = EMPTY;  // Brick type wall's ruin logic 
            }
            if(material != EMPTY){
                *wall_type = map_direction[y][x];
                return material;
            }
        }  
    }
    return EMPTY;
}

uint8_t Bullet_Tank_Check(Bullet_t *bullet, Tank_me *tank){
    AABB bullet_box = {bullet->x - bullet->radius, bullet->y - bullet->radius, bullet->radius*2, bullet->radius*2};
    AABB tank_box = {tank->x, tank->y, tank->size, tank->size};
    if (AABB_Collides(&bullet_box, &tank_box) == 1) {
        return 1;
    }
    return 0;     
}

void Player_Hit_Enemy_Check(Bullet_t *player_bullet, Tank_me *enemy, uint32_t *player_score){
    for (uint8_t i = 0; i < MAX_BULLET; i++) {
        if (player_bullet[i].active == 0){
            continue;
        } 
        for (uint8_t j = 0; j < MAX_ENEMY; j++) {
            if (enemy[j].active == 0){
                continue;
            }
            if (Bullet_Tank_Check(&player_bullet[i], &enemy[j]) == 1) {
                player_bullet[i].active = 0; 
                enemy[j].life = enemy[j].life - 1;
                //-----------voice------------------
                buzzer_tone(&buzzer_cfg, 1000, 30);  
                HAL_Delay(50);  
                buzzer_off(&buzzer_cfg);
                //---------------------------------------
                if (enemy[j].life <= 0) {
                    enemy[j].active = 0;
                    *player_score += 100;
                }   
                break;
            }
        }
    }
}

void Enemy_Hit_Player_Check(Bullet_t *enemy_bullet, Tank_me *player) {
    if (player->active == 0){
        return;
    }
    for (int i = 0; i < MAX_BULLET; i++) {
        if (enemy_bullet[i].active == 0){
            continue;
        } 
        if (Bullet_Tank_Check(&enemy_bullet[i],player) == 1){
            enemy_bullet[i].active = 0; 
            player->life = player->life - 1;   
            //------------voice-------------------------
            buzzer_tone(&buzzer_cfg, 1500, 30);  
            HAL_Delay(50);  
            buzzer_off(&buzzer_cfg);
            //--------------------------------------------
            if (player->life <= 0) {
                player->active = 0;
                current_state = STATE_OVER; 
            }
            break;
        }
    }
}

//==================== Bullet ===============================
void Bullet_player_init(Bullet_t * Bullet,Tank_me * tank){
    //-------------- Fire delay-----------------
    uint32_t current_time = HAL_GetTick();
    if (current_time - tank->shoot_time < FIRE_DELAY){
        return;
    }
    //--------------- initialize ---------------------
    for (int16_t i = 0; i < MAX_BULLET; i++) { 
        if(Bullet[i].active == 0){ 
            Bullet[i].active = 1;
            Bullet[i].dir = tank->dir;
            Bullet[i].radius = 2;
            Bullet[i].color = 2;
            Bullet[i].speed = 6;
            Bullet[i].fire_time = HAL_GetTick();
            Bullet[i].x = tank->x + tank->size / 2;
            Bullet[i].y = tank->y + tank->size / 2;
            tank->shoot_time = current_time;
            break;
        }
    }
}

void Bullet_enemy_init(Bullet_t * Bullet,Tank_me * enemy_one){
    //---------------- Fire delay ---------------------------
    uint32_t current_time = HAL_GetTick();
    if (current_time - enemy_one->shoot_time < FIRE_DELAY_ENEMY){
        return;
    }
    //--------- Set enemies direction of the bullet ----------------
    Direction enemy_direction[4];
    uint16_t count = 0;
    if(enemy_one->dir == N || enemy_one->dir == E){
        enemy_direction[0] = N;
        enemy_direction[1] = S;
        enemy_direction[2] = E;
        enemy_direction[3] = W;
    }
    else{
        enemy_direction[0] = NW;
        enemy_direction[1] = NE;
        enemy_direction[2] = SW;
        enemy_direction[3] = SE;
    }
    //-------- Initialize ---------------------------------
    for (int16_t i = 0; i < MAX_BULLET; i++) {
        if(Bullet[i].active == 0){
            Bullet[i].active = 1;
            Bullet[i].dir = enemy_direction[count];
            Bullet[i].radius = 2;
            Bullet[i].color = 11;
            Bullet[i].speed = 3;
            Bullet[i].fire_time = HAL_GetTick();
            Bullet[i].x = enemy_one->x +enemy_one->size / 2;
            Bullet[i].y = enemy_one->y + enemy_one->size / 2;
            count = count + 1;
            if(count >= 4){
                break;
            }
        }
    }
    enemy_one->shoot_time = current_time;
}

void Bullet_update(Bullet_t * Bullet) {
    uint32_t current_time = HAL_GetTick();
    for(uint16_t i = 0; i < MAX_BULLET; i++) {
        //------------ lifetime of the bullet ---------------
        if (Bullet[i].active == 1) {
            if (current_time - Bullet[i].fire_time >= BULLET_MAX_TIME) {
                Bullet[i].active = 0;
                continue;
            }
        //-------------- Updata the bullet ----------------------------
            int16_t next_x = Bullet[i].x;
            int16_t next_y = Bullet[i].y;

            if (Bullet[i].dir == N || Bullet[i].dir == NW || Bullet[i].dir == NE ){
                 next_y -= Bullet[i].speed;
            }
            if (Bullet[i].dir == S || Bullet[i].dir == SW || Bullet[i].dir == SE ){
                next_y += Bullet[i].speed;
            }
            if (Bullet[i].dir == W || Bullet[i].dir == NW || Bullet[i].dir == SW ){
                next_x -= Bullet[i].speed;
            } 
            if (Bullet[i].dir == E || Bullet[i].dir == NE || Bullet[i].dir == SE ){
                next_x += Bullet[i].speed;
            } 
            uint8_t wall_direction = 0;
            uint16_t rec_x = next_x - Bullet[i].radius;
            uint16_t rec_y = next_y - Bullet[i].radius;
            uint8_t material = Bullet_Wall_Check(rec_x, rec_y, &Bullet[i], &wall_direction);// Find wall's direction and materials
            //--------- Feedback from different materials --------------------
            if (material == EMPTY) {
                Bullet[i].x = next_x; 
                Bullet[i].y = next_y;
            } 
            else  if (material == RUBBER) {
            Bullet_Rubber(&Bullet[i],wall_direction); 
            }
            else if (material == BRICK) {
                Bullet_Brick(&Bullet[i]);  
            }
            else if (material == IRON) {
                Bullet_Iron(&Bullet[i]);   
            }
        }
    }
}

void Bullet_draw(Bullet_t * Bullet) {
    for(uint16_t i = 0; i < MAX_BULLET; i++){
        if (Bullet[i].active == 1){
            LCD_Draw_Circle(Bullet[i].x, Bullet[i].y,Bullet[i].radius, Bullet[i].color, 1);
        }
    }
}

void Bullet_Rubber(Bullet_t *bullet, uint8_t wall_type) {
    if(wall_type == 11){
        switch (bullet->dir){
            case N: bullet->dir = S;  break;
            case S: bullet->dir = N;  break;
            case NW: bullet->dir = SW; break; 
            case NE: bullet->dir = SE; break; 
            case SW: bullet->dir = NW; break; 
            case SE: bullet->dir = NE; break; 
            case E: bullet->dir = W;  break;
            case W: bullet->dir = E;  break;
            default: break;
        }
    }
    else if (wall_type == 12){
        switch(bullet->dir){
            case N: bullet->dir = S;  break;
            case S: bullet->dir = N;  break;
            case E: bullet->dir = W;  break;
            case W: bullet->dir = E;  break;
            case NW: bullet->dir = NE; break; 
            case NE: bullet->dir = NW; break; 
            case SW: bullet->dir = SE; break; 
            case SE: bullet->dir = SW; break;
            default: break;
        }
    }
    else {
        switch (bullet->dir) {
            case N: bullet->dir = S; break;
            case S: bullet->dir = N; break;
            case E: bullet->dir = W; break;
            case W: bullet->dir = E; break;
            case NW: bullet->dir = SE; break;
            case SE: bullet->dir = NW; break;
            case NE: bullet->dir = SW; break;
            case SW: bullet->dir = NE;  break;
            default: break;
        }
    }
}

void Bullet_Brick(Bullet_t *bullet) {
    bullet->active = 0;
}

void Bullet_Iron(Bullet_t *bullet) {
    bullet->active = 0;
}

//------------- enemy -------------------
void Tank_enemy_init(Tank_me * enemy){
    for(uint16_t i = 0; i < MAX_ENEMY; i++){
        enemy[i].x = enemy_start_x[i];
        enemy[i].y = enemy_start_y[i];
        enemy[i].size = 10;
        enemy[i].speed = 1;
        enemy[i].color = 11;
        enemy[i].life = 2;
        enemy[i].active = 1;
        enemy[i].dir = enemy_start_dir[i];
        enemy[i].shoot_time = 0;
    }
}

void Tank_enemy_AI(Tank_me * enemy, Tank_me * tank){
    for(uint16_t i = 0; i < MAX_ENEMY; i++){
        if (enemy[i].active == 0){
            continue;
        }
        //------ Find enemies activity area ------------
        int16_t max_x = 0;
        int16_t min_x  = 0;
        int16_t max_y = 0;
        int16_t min_y = 0;
        if(enemy_end_x[i] > enemy_start_x[i]){
            max_x = enemy_end_x[i];
            min_x  = enemy_start_x[i];
        }
        else{
            max_x = enemy_start_x[i];
            min_x = enemy_end_x[i];
        }
        if(enemy_end_y[i] > enemy_start_y[i]){
            max_y = enemy_end_y[i];
            min_y = enemy_start_y[i];
        }
        else{
            max_y = enemy_start_y[i];
            min_y = enemy_end_y[i];
        }
        //---------- Prevent enemies pass the boundary ---------------
        if (enemy[i].dir == E ){
            if(abs(enemy[i].x - max_x) <= enemy[i].speed){
                enemy[i].x = max_x;
                enemy[i].dir = W;       
            }
        }
        else if (enemy[i].dir == W ){
            if(abs(enemy[i].x -  min_x) <= enemy[i].speed){
                enemy[i].x = min_x;
                enemy[i].dir = E;       
            }
        }
        else if (enemy[i].dir == N ){
            if(abs(enemy[i].y - min_y) <= enemy[i].speed){
                enemy[i].y = min_y;
                enemy[i].dir = S;       
            }
        }
        else if (enemy[i].dir == S ){
            if(abs(enemy[i].y - max_y) <= enemy[i].speed){
                enemy[i].y = max_y;
                enemy[i].dir = N;       
            }
        }
        //---------- Update enemies ----------------------
        int16_t next_x = enemy[i].x;
        int16_t next_y = enemy[i].y;
        if (enemy[i].dir == E) {
            next_x += enemy[i].speed;
        } 
        else if (enemy[i].dir == W) {
            next_x -= enemy[i].speed;
        } 
        else if (enemy[i].dir == S) {
            next_y += enemy[i].speed;
        } 
        else if (enemy[i].dir == N) {
            next_y -= enemy[i].speed;
        }
        //---------- Collision between enemies and players -----------------
        if(tank->active == 1){
            if (Check_player_enemy(next_x, next_y, enemy[i].size, tank) == 1) {
                continue; 
            }
        }
        //------------------------------------------------------------------
        enemy[i].x = next_x;
        enemy[i].y = next_y;
    }
}

void Tank_enemy_draw(Tank_me * enemy){
    uint8_t gun_size = 4; 
    for(uint8_t i = 0; i < MAX_ENEMY ;i++){
        if (enemy[i].active == 0){
            continue;
        }
        uint8_t gun_color = enemy[i].color;
        LCD_Draw_Rect(enemy[i].x, enemy[i].y, enemy[i].size, enemy[i].size, enemy[i].color, 1);
        uint8_t offset = (enemy[i].size - gun_size) / 2;
        if(enemy[i].dir == N || enemy[i].dir == E){
            LCD_Draw_Rect(enemy[i].x + offset, enemy[i].y - gun_size, gun_size, gun_size, gun_color, 1);
            LCD_Draw_Rect(enemy[i].x + enemy[i].size, enemy[i].y + offset, gun_size, gun_size, gun_color, 1);
            LCD_Draw_Rect(enemy[i].x + offset, enemy[i].y + enemy[i].size, gun_size, gun_size, gun_color, 1);
            LCD_Draw_Rect(enemy[i].x - gun_size, enemy[i].y + offset, gun_size, gun_size, gun_color, 1);
        }
        else{
            LCD_Draw_Rect(enemy[i].x - gun_size, enemy[i].y - gun_size, gun_size, gun_size, gun_color, 1); 
            LCD_Draw_Rect(enemy[i].x + enemy[i].size , enemy[i].y - gun_size, gun_size, gun_size, gun_color, 1); 
            LCD_Draw_Rect(enemy[i].x - gun_size, enemy[i].y + enemy[i].size, gun_size, gun_size, gun_color, 1); 
            LCD_Draw_Rect(enemy[i].x + enemy[i].size, enemy[i].y + enemy[i].size, gun_size, gun_size, gun_color, 1); 
        }
    }
}

//======================= LED alarming ============================================
void led_alarm(Tank_me * player, uint32_t *led_alarm, uint8_t *led_state){
    uint32_t current_time = HAL_GetTick();
    if(player->life == 1){
        if ((current_time - *led_alarm) >= 200) {
            *led_alarm = current_time; 
            *led_state = !(*led_state);
            if (*led_state) {
                PWM_SetDuty(&pwm_cfg, 100);
            }
            else {
                PWM_SetDuty(&pwm_cfg, 0);
            }
        }
    }
    else{
        *led_state = 0;
        PWM_SetDuty(&pwm_cfg, 50);
    }

}