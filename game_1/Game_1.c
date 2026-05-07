#include "Game_1.h"
#include "InputHandler.h"
#include "Menu.h"
#include "LCD.h"
#include "PWM.h"
#include "Buzzer.h"
#include "Utils.h"
#include "stm32l4xx_hal.h"
#include "Joystick.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

extern ST7789V2_cfg_t cfg0;
extern PWM_cfg_t pwm_cfg;  // LED PWM control
extern Buzzer_cfg_t buzzer_cfg; // Buzzer control
extern Joystick_cfg_t joystick_cfg;// Joystick control
extern Joystick_t joystick_data;


// Game state variables
static uint32_t game_start_time = 0;
static uint32_t gaming_time = 0;
static uint32_t last_generation_time = 0;
static int32_t generation_interval = 0;
static int16_t moving_x = 0;
static int16_t moving_y = 0;
static int16_t road_move = 0;
static uint8_t game_start = 0;
#define MAX_ENEMY 5 
static int16_t enemy_x[MAX_ENEMY];
static int16_t enemy_y[MAX_ENEMY];
static int16_t enemy_active[MAX_ENEMY] = {0};
static int8_t same_lane_count = 0;
static int8_t last_lane = -1;

#define CAR_WIDTH 39
#define CAR_HEIGHT 28


// Frame rate for this game (in milliseconds)
#define GAME1_FRAME_TIME_MS 30  

// Road
void road(int road_move){
    for(int y = 40; y < 240; y += 30){
        LCD_Draw_Rect(80,  y + road_move, 4, 16, 1, 1);
        LCD_Draw_Rect(160, y + road_move, 4, 16, 1, 1);
    }
}

// Enemy car
void enemy_car(int x, int y){
    LCD_Draw_Rect(x + 5,  y + 8,  30, 18, 11,1);
    LCD_Draw_Rect(x + 10,  y + 3,  20,  5, 11,1);
    LCD_Draw_Rect(x + 10,  y + 26, 20,  5, 11,1);
    LCD_Draw_Rect(x + 12, y + 12, 16,  6, 1,1);
    LCD_Draw_Rect(x,  y + 3,  5,  6, 1,1);
    LCD_Draw_Rect(x,  y + 26,  5,  6, 1,1);
    LCD_Draw_Rect(x + 35, y + 3,  5,  6, 1,1);
    LCD_Draw_Rect(x + 35, y + 26,  5,  6, 1,1);
}

// Player's car
void player_car(int x, int y){
    LCD_Draw_Rect(x + 5,  y + 8,  30, 18, 2,1);
    LCD_Draw_Rect(x + 10,  y + 3,  20,  5, 2,1);
    LCD_Draw_Rect(x + 10,  y + 26, 20,  5, 2,1);
    LCD_Draw_Rect(x + 12, y + 12, 16,  6, 1,1);
    LCD_Draw_Rect(x,  y + 3,  5,  6, 1,1);
    LCD_Draw_Rect(x,  y + 26,  5,  6, 1,1);
    LCD_Draw_Rect(x + 35, y + 3,  5,  6, 1,1);
    LCD_Draw_Rect(x + 35, y + 26,  5,  6, 1,1);

}
MenuState Game1_Run(void) {
    // Initialize game state
    // Player racing initial position
    moving_x = 120;
    moving_y = 120;

    game_start_time = HAL_GetTick(); // Game start time
    last_generation_time = HAL_GetTick(); // Last enemy car generation time

    // Clear the enemy
    for(int i = 0; i < MAX_ENEMY; i++){
        enemy_active[i] = 0;
        enemy_x[i] = 0;
        enemy_y[i] = 0;
    }

    game_start = 0; //The game not start

    srand(HAL_GetTick()); // random seed

    MenuState exit_state = MENU_STATE_HOME;  // Default: return to menu
    
    // Game's own loop - runs until exit condition
    while (1) {
        uint32_t frame_start = HAL_GetTick();
        
        // Read input
        Input_Read();

        // Game start interface
        if(!game_start){
            LCD_Fill_Buffer(0);

            // Display the game start interface text
            LCD_printString("Racing Evasion", 30, 40, 1, 2);
            LCD_printString("Press BT2 to exit", 20, 140, 1, 2);
            LCD_printString("Press BT3 to start", 20, 100, 1, 2);

            LCD_Refresh(&cfg0);

            // Check if button was pressed to start the game
            if(current_input.btn3_pressed){
                game_start = 1;

                //Reset time
                game_start_time = HAL_GetTick();
                last_generation_time = HAL_GetTick();
                
                //Reset track count
                same_lane_count = 0;
                last_lane = -1;
            }

            // Check if button was pressed to return to menu
            if (current_input.btn2_pressed) {
                exit_state = MENU_STATE_HOME;
                break;  // Exit game loop
            }

            HAL_Delay(GAME1_FRAME_TIME_MS);
            continue; 
        }
        
        
        // Check if button was pressed to return to menu
        if (current_input.btn2_pressed) {
            exit_state = MENU_STATE_HOME;
            break;  // Exit game loop
        }
        

        
        // Player car movement
        Joystick_Read(&joystick_cfg,&joystick_data);
        UserInput input = Joystick_GetInput(&joystick_data);
        switch(input.direction){
            case N:
            moving_y -= 3;
            break;

            case S:
            moving_y += 3;
            break;

            case E:
            moving_x += 3;
            break;

            case W:
            moving_x -= 3;
            break;

            case NE:
            moving_x += 3;
            moving_y -= 3;
            break;

            case NW:
            moving_x -= 3;
            moving_y -= 3;
            break;

            case SE:
            moving_x += 3;
            moving_y += 3;
            break;

            case SW:
            moving_x -= 3;
            moving_y += 3;
            break;

            default:
            break;

        }

        // Restricting player's car from leaving the boundary
        if(moving_x < 0){
            moving_x = 0;
        }
        if(moving_x > 200){
            moving_x = 200;
        }
        if(moving_y < 1){
            moving_y = 1;
        }
        if(moving_y > 200){
            moving_y = 200;
        }

        // RENDER: Draw to LCD
        LCD_Fill_Buffer(0);
        // Title
        LCD_printString("Racing Evasion", 30, 10, 1, 2);

        // Timer
        gaming_time = (HAL_GetTick() - game_start_time) / 1000;
        char time_str[32];
        sprintf(time_str, "Time: %lus", gaming_time);
        LCD_printString(time_str, 0, 30, 1, 2);
        
        //  Road animation
        road_move += 2;
        if(road_move >= 30){
            road_move = 0;
        }
        road(road_move);

        // Enemy car generation interval
        generation_interval = 2500 - gaming_time * 20;
        if(generation_interval < 800){
            generation_interval = 800;
        }

        // Generate enemy cars
        if(HAL_GetTick() - last_generation_time >= generation_interval){

            // Randomly select one lane from three lanes
            int r = rand() % 3;

            // Max 2 consecutive generations in the same lane
            if(same_lane_count >= 2){
                while(r == last_lane){
                    r = rand() % 3;
                }
            }
            int lane;
            if(r == 0){
                lane = 27;
            }else if(r == 1){
                lane = 104;
            }else if(r == 2){
                lane = 181;
            }

            //Control the distance between two car 
            int crowded = 0;
            for(int i = 0; i < MAX_ENEMY; i++){
                if(enemy_active[i] == 1){
                    if(enemy_x[i] != lane && enemy_y[i] < 65){
                        crowded = 1;
                        break;
                    }
                    
                }
            
            if(enemy_active[i] == 1){
                    if(enemy_x[i] == lane && enemy_y[i] < 30){
                        crowded = 1;
                        break;
                    }
                    
                }
            }

            // Cars are generated in the appropriate distance
            if(!crowded){
                for(int i = 0; i < MAX_ENEMY; i++){
                    if(enemy_active[i] == 0){
                        enemy_active[i] = 1;
                        enemy_y[i] = 0; 
                        enemy_x[i] = lane;   
                        break;
                        
                    }
                }

                //Update the number of consecutive cars on the lane
                if(r == last_lane){
                    same_lane_count++;
                }else{
                    last_lane = r;
                    same_lane_count = 1;
                }
                last_generation_time = HAL_GetTick(); // Update generation time
            }
        
 
        }

        // Enemy car movement
        for(int i = 0; i < MAX_ENEMY; i++){
            if(enemy_active[i] == 1){
                int enemy_speed;
                enemy_speed = 2 + gaming_time / 50;
                if(enemy_speed > 4){
                    enemy_speed = 4;
                }
                enemy_y[i] += enemy_speed;
                if(enemy_y[i] > 240){
                    enemy_active[i] = 0;
                }
            }
        }

        // y >= 40 draw enemy cars
        for(int i = 0; i < MAX_ENEMY; i++){
            if(enemy_active[i] == 1){
                if(enemy_y[i] >= 40){
                    enemy_car(enemy_x[i], enemy_y[i]);
                }
            }
        }

        // Collision detection
        uint8_t collision = 0;
        AABB player_box = {moving_x, moving_y, CAR_WIDTH, CAR_HEIGHT};
        for(int i = 0; i < MAX_ENEMY; i++){
            if(enemy_active[i] == 1){
                AABB enemy_box = {enemy_x[i], enemy_y[i], CAR_WIDTH, CAR_HEIGHT};
                if(AABB_Collides(&player_box, &enemy_box)){
                    collision = 1;
                    break;
                }
            }
        }

        // Game over
        if(collision){
            LCD_Draw_Circle(moving_x + 19, moving_y + 14, 20, 5, 1);
            LCD_Draw_Circle(moving_x + 19, moving_y + 14, 10, 2, 1);
            LCD_Refresh(&cfg0);

            buzzer_tone(&buzzer_cfg, 2000, 50);
            PWM_SetDuty(&pwm_cfg, 0);
            HAL_Delay(200);
            PWM_SetDuty(&pwm_cfg, 100);
            HAL_Delay(200);
            PWM_SetDuty(&pwm_cfg, 0);
            HAL_Delay(200);
            PWM_SetDuty(&pwm_cfg, 100);
            HAL_Delay(200);
            PWM_SetDuty(&pwm_cfg, 0);
            HAL_Delay(200);
            PWM_SetDuty(&pwm_cfg, 100);
            buzzer_off(&buzzer_cfg);

            while(1){
                Input_Read();
                LCD_Fill_Buffer(0);
                LCD_printString("Game Over", 40, 70, 1, 3);

                char time_str[32];
                sprintf(time_str, "Survival: %lus", gaming_time);
                LCD_printString(time_str, 40, 110, 1, 2);
                LCD_printString("Press BT2 to exit",20 ,140,1,2);
                LCD_Refresh(&cfg0);
                
                // Check if button was pressed to return to menu    
                if (current_input.btn2_pressed) {
                     exit_state = MENU_STATE_HOME;
                     break;  
                }   
            }
            break;
        }

        // Draw player cars
        player_car(moving_x, moving_y);


        LCD_Refresh(&cfg0);
        
        // Frame timing - wait for remainder of frame time
        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < GAME1_FRAME_TIME_MS) {
            HAL_Delay(GAME1_FRAME_TIME_MS - frame_time);
        }
    }
    
    return exit_state;  // Tell main where to go next
}