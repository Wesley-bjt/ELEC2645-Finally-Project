#include "Game_3.h"
#include "InputHandler.h"
#include "Joystick.h"
#include "Menu.h"
#include "LCD.h"
#include "PWM.h"
#include "Buzzer.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdint.h>
#include "Utils.h"

extern ST7789V2_cfg_t cfg0;
extern PWM_cfg_t pwm_cfg;      // LED PWM control
extern Buzzer_cfg_t buzzer_cfg; // Buzzer control
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;

#define GAME3_FRAME_TIME_MS 16  // ~60 FPS
#define MIN_X 0 // the minimum x boundary
#define MAX_X 220 // the maximum x boundary
#define MAX_PLATFORM 20 // how maximum number of platform that show in the screen
#define PLATFORM_WIDTH 40 //width of platform
#define PLATFORM_HEIGHT 5 //height of platform
#define MAX_GRAVITY 10 // the fastest drop speed the character could get

//------------variable declear-------------------------
//------------platform generate and other game parameter-----------
static uint16_t score = 0;
static uint16_t current_platform = -1;
static int16_t dead_line;
static int16_t last_p_y;
static int16_t last_p_x;
static int16_t distance_d_to_p;

//--------timer-----------------
static int16_t buzzer_start_time = 0;
static int16_t buzzer_duration = 0;
static uint8_t buzzer_active = 0;

static int16_t LED_timer = 0;
static uint8_t LED_state = 0;

//--------buzzer------
void buzzer(uint16_t frequency, uint8_t volume, int16_t duration){
    buzzer_tone(&buzzer_cfg, frequency, volume);
    buzzer_start_time = HAL_GetTick();
    buzzer_duration = duration;
    buzzer_active =1;
}

void buzzer_update(){
    if(buzzer_active == 1){
        if(HAL_GetTick() - buzzer_start_time >= buzzer_duration){
            buzzer_off(&buzzer_cfg);
            buzzer_active = 0;
        }
    }
}

//voices used in game
void voice_game_start(){
    buzzer(800, 50, 10);
}

void voice_jump(){
    buzzer(400, 50, 5);
}

void voice_game_over(){
    for(int i = 1200; i > 400; i -= 40){
        buzzer(i,70,10);
        HAL_Delay(10);
    }
}

//blinking LED
void alarm(){
    if(HAL_GetTick() - LED_timer >= 50){
        LED_timer = HAL_GetTick();
        LED_state = !LED_state;

        if(LED_state == 1){
            PWM_SetDuty(&pwm_cfg, 100);
        }else {
            PWM_SetDuty(&pwm_cfg, 0);
        }
    }
}

//menu design
typedef enum{
    s_Main_menu,
    s_Game,
    s_Game_over
} game_3_state;

game_3_state state;

void Main_menu_render(){
    LCD_Fill_Buffer(0);

    LCD_printString("Jumper", 20, 40, 1, 5);
    LCD_printString("Press BT3 to start",20,100,1,2);
    LCD_printString("Press BT2 to exist", 20, 120, 1, 2);

    LCD_Refresh(&cfg0);
}

//create a player and define its attributes
typedef struct{
    int16_t x;
    int16_t y;
    float v_x;
    float v_y;
    float gravity;
    float friction;
    float air_friction;
    float move_speed;
    float max_speed;
    float jump_speed;
    uint8_t max_jump;
    uint8_t left_jump;
    uint8_t on_ground;
    int8_t buff_active;
    uint16_t buff_timer;
}Player_attribute;

static Player_attribute Player;

//player initialized
void Player_init(void){
    Player.x = 110;
    Player.y = 200;
    Player.v_x = 0;
    Player.v_y = 0;
    Player.gravity = 1.0f;
    Player.friction = 0.7f;
    Player.air_friction = 0.95f;
    Player.move_speed = 1.5f;
    Player.max_speed = 5.0f;
    Player.jump_speed = -15.0f;
    Player.max_jump = 1;
    Player.left_jump = 1;
    Player.on_ground = 1;
    dead_line = 260;
    Player.buff_active = 0;
    Player.buff_timer = 0;
}

//define platforms and its variants
typedef enum{
    Platform_static,
    Platform_moving,
    Platform_breakable,
    Platform_buff,
    Platform_fake
} PlatformType;

typedef struct{
    int16_t x;
    int16_t y;
    int16_t width;
    uint8_t active;
    PlatformType type;
    float v_x;
    uint8_t life; 
}Platform ;

static Platform platforms[MAX_PLATFORM];

//platforms initialized
void Platform_init(){
    last_p_y = Player.y;
    last_p_x = Player.x;
    
    for(int i = 0; i < MAX_PLATFORM; i++){
        platforms[i].active = 0;
    }
    platforms[0].x = Player.x - 10;
    platforms[0].y = Player.y + 10;
    platforms[0].width = PLATFORM_WIDTH;
    platforms[0].active = 1;
    //generate an initial platform

}

//collision detection
void Collision_detection(){
    if(Player.v_y > 0){// only detection when player drop
        AABB Player_collision = {Player.x,Player.y,10,10};
        for(int i = 0; i < MAX_PLATFORM; i++){
            if(platforms[i].active == 1){
                AABB Platform_collision = {platforms[i].x,platforms[i].y,platforms[i].width,PLATFORM_HEIGHT};

                if(AABB_Collides(&Player_collision, &Platform_collision) == 1){

                    if(platforms[i].type == Platform_fake){
                        continue;
                    }// just ignore the fake platform

                    Player.on_ground = 1;
                    Player.y = platforms[i].y - 10;
                    Player.v_y = 0;
                    Player.left_jump = Player.max_jump;
                    //set the attributes when character on ground

                    current_platform = i;

                    if(platforms[i].type == Platform_breakable){
                        platforms[i].life -= 1;
                        if(platforms[i].life == 0){
                        platforms[i].active = 0;
                        }
                    }//break the breakable platform when detect the breakable platform
                    

                    if(platforms[i].type == Platform_buff){
                        Player.buff_active = 1;
                        Player.buff_timer = 60;
                        Player.move_speed = 3.0f;
                        Player.max_speed = 7.0f;
                        Player.jump_speed = -20.0f;
                        Player.max_jump = 2;

                    }//active the buff when detect the buff platform
                    break;
                }
            }
        }

    }
}

//define how player update
void Player_update(UserInput input){
    if(input.direction == E ||input.direction == NE ||input.direction == SE){
        Player.v_x += Player.move_speed;
    }else if (input.direction == W||input.direction == NW ||input.direction == SW) {
        Player.v_x -= Player.move_speed;
    }// move horizontally when joystick pressed on left or right
    
    if(Player.on_ground == 1){
        Player.v_x *= Player.friction;
    }else {
        Player.v_x *= Player.air_friction;
    }//use different friction when player is on ground or in air

    if(Player.v_x >= Player.max_speed){
        Player.v_x = Player.max_speed;
    }
    if(Player.v_x <= -Player.max_speed){
        Player.v_x = -Player.max_speed;
    }//speed limit

    //edge detection when input direction is upward
    //prevent the character fly upward infinitely
    //character only will jump once even the joystick is keeping pressed upward
    static uint8_t jump_pressed_last = 0;
    uint8_t jump_pressed_now = (input.direction == N);
    if(jump_pressed_now && !jump_pressed_last && Player.left_jump >0){
        Player.v_y = Player.jump_speed;
        Player.left_jump -= 1;
        voice_jump();
    }
    jump_pressed_last = jump_pressed_now;
    
    Player.v_y += Player.gravity;//character continuously effect by gravity

    if(Player.v_y > MAX_GRAVITY){
        Player.v_y = MAX_GRAVITY;
    }//speed limit in vertical moving
    
    Player.x += (int16_t)Player.v_x;
    Player.y += (int16_t)Player.v_y;
    //update the player location

    Player.on_ground = 0;

    current_platform = -1;

    Collision_detection();

    if(Player.on_ground && current_platform != -1){
           if(platforms[current_platform].type == Platform_moving){
           Player.x += platforms[current_platform].v_x;
        }
    }//character will move with moving platform when it stand on the moving platform

    if(Player.buff_active == 1){
        Player.buff_timer -= 1;

        if(Player.buff_timer <= 0){
            Player.move_speed = 1.5f;
            Player.max_speed = 5.0f;
            Player.jump_speed = -15.0f;
            Player.max_jump = 1;
            Player.buff_active = 0;
        }//reset the attributes after buff timer is over
    }
    
    if(Player.x <= MIN_X){
        Player.x = MIN_X;
    }
    if(Player.x >= MAX_X){
        Player.x = MAX_X;
    }//horizontal limit
}

//extra spawn platform
void Extra_spawn(int16_t main_p_y){
    int16_t max_extra = 3 - score / 3000;
    if(max_extra < 0){
        max_extra = 0;
    }//extra spawn number will be limit and will decrease withe score increase

    int8_t extra_count = Random_U16(max_extra + 1);

    int16_t spawn_chance = 90 - (score / 20);
    if(spawn_chance < 50){
        spawn_chance = 50;
    }//limit the spawn chance

    //extra platform generate code
    for(int a = 0; a < extra_count; a++){
        if(Random_U16(100) < spawn_chance){
            //probability of generation or non-generation
            for(int i = 0; i < MAX_PLATFORM; i++){
                if(platforms[i].active == 0){
                    platforms[i].active = 1;
                    platforms[i].x = Random_U16(MAX_X - PLATFORM_WIDTH);//random generate in x-axis
                    platforms[i].y = main_p_y + (Random_U16(50) - 25);//generate around main platform
                    platforms[i].width = PLATFORM_WIDTH;

                    int16_t r = Random_U16(100);

                    //set the generate weight of platform type
                    int16_t p_static = 100;
                    int16_t p_breakable = 0;
                    int16_t p_moving = 0;
                    int16_t p_fake = 0;
                    int16_t p_buff = 0;

                    //lock the generation of the variant until the generation conditions are met
                    //the probability of variant platform generate will increase as the score increase
                    if(score > 500){
                        p_breakable = 10 + (score - 500) / 100;
                        if(p_breakable > 35){
                            p_breakable = 35;
                        }
                    }

                    if(score > 1000){
                        p_moving = 10 + (score - 1000) / 100;
                        if(p_moving > 25){
                            p_moving = 25;
                        }
                    }

                    if(score > 2000){
                        p_buff = 0 + (score - 2000) / 100;
                        if(p_buff > 8){
                            p_buff = 8;
                        }
                    }

                    if(score > 2500){
                        p_fake = 0 + (score - 2500) / 100;
                        if(p_fake > 10){
                            p_fake = 10;
                        }
                    }

                    p_static = 100 - (p_breakable + p_moving + p_buff + p_fake);

                    //random generate the platorms by their weight
                    int16_t sum1 = p_static;
                    int16_t sum2 = sum1 + p_breakable;
                    int16_t sum3 = sum2 + p_moving;
                    int16_t sum4 = sum3 + p_buff;
                    int16_t sum5 = sum4 + p_fake;

                    if(r < sum1){
                        platforms[i].type = Platform_static;
                    }else if(r < sum2){
                        platforms[i].type = Platform_breakable;
                        platforms[i].life = 1;
                    }else if(r < sum3){
                        platforms[i].type =Platform_moving;
                        platforms[i].v_x = 1 + score / 1000;
                    }else if(r < sum4){
                        platforms[i].type = Platform_buff;
                    }else if(r < sum5){
                        platforms[i].type = Platform_fake;
                    }else{
                        platforms[i].type = Platform_static;
                    }

                    break;

                }
            }
        }
    }
}


//spawn the main platform
//the generation of the main platform must be restrict
//to prevent all the platforms spawn in the place player coulden't get
void Platform_spawn(){
    //limit the gap between the generate platforms
    //the gap will increase with the score
    int16_t gap_min_y = 20;
    int16_t gap_max_y = 40;

    gap_min_y += score / 200;
    gap_max_y += score / 400;

    int16_t gap_y = gap_min_y + Random_U16(gap_max_y -gap_min_y);
    int16_t new_y = last_p_y - gap_y;// each platform will generate above the previous one

    int16_t gap_x = Random_U16(150) - 75;
    int16_t new_x = last_p_x - gap_x;// the horizontal distance between each new generated platform is in range [-75,75]

    if(new_x < 0){
        new_x = 0;
    }
    if(new_x > MAX_X - PLATFORM_WIDTH){
        new_x = MAX_X - PLATFORM_WIDTH;
    }//prevent the new platforms generate out of the sides

    uint8_t spawned = 0;

    for(int i = 0; i < MAX_PLATFORM; i++){
        if(platforms[i].active == 0){
            platforms[i].active = 1;
            platforms[i].x = new_x;
            platforms[i].y = new_y;
            platforms[i].width = PLATFORM_WIDTH;

            //Platforms and variations generated based on the proportion that changes with the score

            uint16_t p_static = 100;
            uint16_t p_breakable = 0;
            uint16_t p_moving = 0;

            if(score > 1000){
                p_breakable = 10 + (score - 1500) / 100;
                if(p_breakable > 45){
                    p_breakable = 45;
                }
            }

            if(score > 2000){
                p_moving = 10 + (score - 2000) / 100;
                if(p_moving > 25){
                    p_moving = 25;
                }
            }

            p_static = 100 - (p_breakable + p_moving);

            int16_t type_r =Random_U16(100);


            uint16_t sum1 = p_static;
            uint16_t sum2 = sum1 + p_breakable;
            uint16_t sum3 = sum2 + p_moving;

            if(type_r < sum1){
                platforms[i].type = Platform_static;
            }else if(type_r < sum2){
                platforms[i].type = Platform_breakable;
                platforms[i].life = 1;
            }else if(type_r < sum3){
                platforms[i].type =Platform_moving;
                platforms[i].v_x = 1 + score / 1000;
            }else{
                platforms[i].type = Platform_static;
            }

            spawned = 1;

            break;

        }
    }
    
    if(spawned == 1){
        last_p_x = new_x;
        last_p_y = new_y;
        Extra_spawn(new_y);
    }//the current platform will become "last platform"
    //new platform will generate around it
}


//platform update
void Platform_update(){
    for(int i = 0; i < MAX_PLATFORM; i++){
        if(platforms[i].active == 1 && platforms[i].y >= 240){
            platforms[i].active = 0;  
        }
    }//the platforms that fall out of the screen will be clean
    //and leave the space for new platform

    if(last_p_y > Player.y - 240){
        Platform_spawn();
    }//if there is still space above player, continue generate

    for(int i = 0;i < MAX_PLATFORM; i++){
        if(platforms[i].type == Platform_moving && platforms[i].active == 1){
            platforms[i].x += platforms[i].v_x;

            if(platforms[i].x <= 0){
                platforms[i].v_x = -platforms[i].v_x;
            }else if(platforms[i].x >= MAX_X - platforms[i].width){
                platforms[i].x = MAX_X - platforms[i].width;
                platforms[i].v_x = -platforms[i].v_x;
            }
        }
    }//the moving platform will keep moving, and drive the character above

}


//the map will move when character move like it is infinite in height
void Map_scroll(){
    if(Player.y < 80){
        int16_t offset = 80 - Player.y;
        Player.y = 80;
        score += offset;//score = the height character climb
        dead_line += offset;

        for(int i = 0; i < MAX_PLATFORM; i++){
            platforms[i].y += offset;
        }

        last_p_y += offset;
    }//when player move upward, everything will move too


}


//collect the functions about the game play
void Game(UserInput input){
        Player_update(input);
        Map_scroll();
        Platform_update();


        //add the chasing dead line
        float dead_line_speed = 0.1f + score / 1500.0f;//dead line's speed will increase with score

        if(dead_line_speed > 6.0f && score < 5000){
            dead_line_speed = 6.0f;
        }else if(dead_line_speed > 8.5f){
            dead_line_speed = 8.5f;
        }//the speed limit of the dead line

        dead_line -= dead_line_speed;

        if(Player.y > 240 || Player.y > dead_line){
            state = s_Game_over;
            voice_game_over();
        }//game over if player fall out of the screen  or catch up by the dead line

        if(dead_line < 240){
            alarm();
        }else {
            PWM_SetDuty(&pwm_cfg, 100);
        }//if dead line near to the character, active the blinking LED(alarm)

        distance_d_to_p = dead_line - Player.y;
}

void Game_render(){
    LCD_Fill_Buffer(0);
    for(int i = 0; i < MAX_PLATFORM; i++){
        if(platforms[i].active == 1){
            if(platforms[i].type == Platform_static){
                LCD_Draw_Rect(platforms[i].x, platforms[i].y, platforms[i].width, PLATFORM_HEIGHT, 1, 1);
            }else if(platforms[i].type == Platform_moving){
                LCD_Draw_Rect(platforms[i].x, platforms[i].y, platforms[i].width, PLATFORM_HEIGHT, 4, 1);
            }else if(platforms[i].type == Platform_breakable){
                LCD_Draw_Rect(platforms[i].x, platforms[i].y, platforms[i].width, PLATFORM_HEIGHT, 5, 1);
            }else if(platforms[i].type == Platform_fake){
                LCD_Draw_Rect(platforms[i].x, platforms[i].y, platforms[i].width, PLATFORM_HEIGHT, 13, 1);
            }else if(platforms[i].type == Platform_buff){
                LCD_Draw_Rect(platforms[i].x, platforms[i].y, platforms[i].width, PLATFORM_HEIGHT, 2, 1);
            }//fill the color according to the platform type
            
        }
    }
    if(dead_line < 240){
        LCD_Draw_Rect(0, dead_line, 240, 1, 2, 1);
    }//draw the dead line

    //HUD render
    char score_render[20];
    sprintf(score_render, "Score:%d", score);
    char dead_line_distance_render[20];
    sprintf(dead_line_distance_render, "dead line:%d", distance_d_to_p);
    LCD_printString(score_render, 0, 5, 1, 2);
    LCD_printString(dead_line_distance_render, 0, 19, 1, 2);
    if(Player.buff_active == 1){
        LCD_Draw_Rect(Player.x,Player.y,10,10,2,1);
    }else {
        LCD_Draw_Rect(Player.x,Player.y,10,10,1,1);
    }//character will be red if it is buffed
    LCD_Refresh(&cfg0);
}

//game over render
void Game_over_menu_render(){
    char result[20];
    LCD_Fill_Buffer(0);
    LCD_printString("Game Over", 20, 80, 1, 4);
    sprintf(result, "Score : %d", score);
    LCD_printString(result, 10, 130, 1, 2);
    LCD_printString("Press BT2 to ", 10, 150, 1, 2);
    LCD_printString("restart", 10, 170, 1, 2);
    LCD_Refresh(&cfg0);
}



//main program
MenuState Game3_Run() {
    state = s_Main_menu;
    MenuState exit_state = MENU_STATE_HOME;
    LCD_Set_Palette(PALETTE_DEFAULT);
    while(1){
        buzzer_update();
        uint32_t frame_start = HAL_GetTick();
        Input_Read();
        Joystick_Read(&joystick_cfg,&joystick_data);
        UserInput input = Joystick_GetInput(&joystick_data);
        switch (state) {
            case s_Main_menu:
            Main_menu_render();
            if(current_input.btn3_pressed){
                Player_init();
                Platform_init();
                score = 0;
                state = s_Game;
                voice_game_start();
            }

            if(current_input.btn2_pressed){
                return MENU_STATE_HOME;
                break;
            }//press botton 2 to exist or botton 3 to start the game
            break;
            
            case s_Game:
            Game(input);
            Game_render();
            break;


            case s_Game_over:
            Game_over_menu_render();
            if(current_input.btn2_pressed){
                Player_init();
                Platform_init();
                score = 0;
                state = s_Main_menu;
            }//press botton 2 to back to the main menu
            break;



        
        }


        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < GAME3_FRAME_TIME_MS) {
            HAL_Delay(GAME3_FRAME_TIME_MS - frame_time);
        }

        

    }
    return exit_state;

}
