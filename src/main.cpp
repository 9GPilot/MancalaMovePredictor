/**
 * File: main.cpp [MASTERs version]
 * Author: Alex Myers
 * Purpose: This ESP32 will receive betting data from another ESP32 and this ESP32 will 
 * display that data.
 */

/*  Install the "lvgl" library version 9.2 by kisvegabor to interface with the TFT Display - https://lvgl.io/
    *** IMPORTANT: lv_conf.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE lv_conf.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd-lvgl/ or https://RandomNerdTutorials.com/esp32-tft-lvgl/   */
#include <lvgl.h>

/*  Install the "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
    *** IMPORTANT: User_Setup.h available on the internet will probably NOT work with the examples available at Random Nerd Tutorials ***
    *** YOU MUST USE THE User_Setup.h FILE PROVIDED IN THE LINK BELOW IN ORDER TO USE THE EXAMPLES FROM RANDOM NERD TUTORIALS ***
    FULL INSTRUCTIONS AVAILABLE ON HOW CONFIGURE THE LIBRARY: https://RandomNerdTutorials.com/cyd-lvgl/ or https://RandomNerdTutorials.com/esp32-tft-lvgl/   */
#include <TFT_eSPI.h>

// Install the "XPT2046_Touchscreen" library by Paul Stoffregen to use the Touchscreen - https://github.com/PaulStoffregen/XPT2046_Touchscreen - Note: this library doesn't require further configuration
#include <XPT2046_Touchscreen_TT.h>
#include <WiFi.h>
#include <esp_now.h>


// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;

// My Global Variables
  int ti;


// My LVGL Objects
  static lv_obj_t *debug_label;


#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// If logging is enabled, it will inform the user about what is happening in the library
void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// Get the Touchscreen data
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  // Checks if Touchscreen was touched, and prints X, Y and Pressure (Z)
  if(touchscreen.tirqTouched() && touchscreen.touched()) {
    // Get Touchscreen points
    TS_Point p = touchscreen.getPoint();
    // Calibrate Touchscreen points with map function to the correct width and height
    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;

    data->state = LV_INDEV_STATE_PRESSED;

    // Set the coordinates
    data->point.x = x;
    data->point.y = y;

    // Print Touchscreen info about X, Y and Pressure (Z) on the Serial Monitor
    /* Serial.print("X = ");
    Serial.print(x);
    Serial.print(" | Y = ");
    Serial.print(y);
    Serial.print(" | Pressure = ");
    Serial.print(z);
    Serial.println();*/
  }
  else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// Manacala Struct
  typedef struct mancala_board {
    char playerToMove; // its the player to take the next move 
    uint8_t player_A_goal;
    uint8_t player_A_board_bins[6];
    
    uint8_t player_B_goal;
    uint8_t player_B_board_bins[6];
  } mancala_board;


// PERSONAL FUNCTIONS
  void printMancalaBoard(mancala_board board){
    Serial.printf(" B [%u] [%u]<[%u] [%u]<[%u] [%u]\n", board.player_B_board_bins[5], board.player_B_board_bins[4], board.player_B_board_bins[3], board.player_B_board_bins[2], board.player_B_board_bins[1], board.player_B_board_bins[0]); 
    Serial.printf("[%u]     Next Player: %c     \n", board.player_B_goal, board.playerToMove);
    Serial.printf("                           [%u]\n", board.player_A_goal);
    Serial.printf("   [%u] [%u]>[%u] [%u]>[%u] [%u]  A\n", board.player_A_board_bins[0], board.player_A_board_bins[1], board.player_A_board_bins[2], board.player_A_board_bins[3], board.player_A_board_bins[4], board.player_A_board_bins[5]); 
  }

  void debug_msg(lv_obj_t *debug_label, const char* text){
    lv_label_set_text(debug_label, text);
  }

// Mancala Tree Functions
  /**
   * simulate_mancala_move(board): This function will be given a manacala board state,
   * and the index we must simulate the move from on the playerToMoves side of the board.
   * 
   * Parameters:
   *  board (mancala_board): The board state holding playerToMove, and the totals for all bins
   *  move_pos_idx (uint8_t): The index on the playerToMoves bins that we are simulating a move to start from 
   * 
   * Return:
   *  mancala_board: A modified version of the original board given that has all the bins updated to reflect the player moving from the 
   *                  move_pos_idx given.
   */
  mancala_board simulate_mancala_move(mancala_board board, int8_t move_pos_idx){
    // Error Check: if move_pos_idx is <0 or >5 then report error
    if (move_pos_idx < 0 || move_pos_idx > 5){
      Serial.printf("!Bug! movePosIdx<0||>5\n");
      debug_msg(debug_label, "!Bug! movePosIdx<0||>5\n"); // TODO I have to make this function
      mancala_board empty_board;
      empty_board.playerToMove = 'X';
      return empty_board;
    }

    uint8_t *first_bins;
    uint8_t *second_bins;
    uint8_t *goal_ptr;
    char other_player;
    if (board.playerToMove == 'A'){
      first_bins = board.player_A_board_bins;
      second_bins = board.player_B_board_bins;
      goal_ptr = &board.player_A_goal;
      other_player = 'B';
    } else{
      first_bins = board.player_B_board_bins;
      second_bins = board.player_A_board_bins;
      goal_ptr = &board.player_B_goal;
      other_player = 'A';
    }

    // Error Check: if the player chose a bin with zero, report error, GUI shouldnt display those 
    if (first_bins[move_pos_idx]==0){
      Serial.printf("!Bug! movePosIdx=empty bin");
      debug_msg(debug_label, "!Bug! movePosIdx=empty bin");
      mancala_board empty_board;
      empty_board.playerToMove = 'X';
      return empty_board;
    }

    // Take out the bin the player chose
    uint8_t amt_in_hands = first_bins[move_pos_idx];
    first_bins[move_pos_idx] = 0;

    bool use_scnd_bins = false; // Boolean flag to denote we are on the other side

    while (amt_in_hands != 0){
      // if move_pos_idx +1 equals 6 that means we are in the first persons goal 
      if (move_pos_idx+1 == 6){ // if we are in our goal, add one to goal
        if (use_scnd_bins == false){
          *goal_ptr += 1;
          amt_in_hands -= 1;
          use_scnd_bins = true; // switch to other side
          move_pos_idx = -1; // resetting to start of otherside, we do -1 beacuse we plus one later
          continue;
        }
        else{ // swap over to the other side of the board, skipping other players goals
          use_scnd_bins = false;
          move_pos_idx = -1;
          continue;
        }
      }

      // move over a index, drop one 
      move_pos_idx += 1;
      if (use_scnd_bins == false){
        first_bins[move_pos_idx] += 1;
        // if we droped out last marble here and it wasnt a empty bin then pick them back up 
        if (amt_in_hands == 1 && first_bins[move_pos_idx] > 1){
          amt_in_hands = first_bins[move_pos_idx];
          first_bins[move_pos_idx] = 0;
          continue;
        }

      } else if (use_scnd_bins == true){
        second_bins[move_pos_idx] += 1;
        // if we dropped out last marble here and it wasnt empty bin, then pick them up and keep moving
        if (amt_in_hands == 1 && second_bins[move_pos_idx] > 1){ // we check if greater than 1 because we just added 1
          amt_in_hands = second_bins[move_pos_idx];
          second_bins[move_pos_idx] = 0;
          continue;
        }
      }
      amt_in_hands -= 1;
   }
   // if use_scnd_bins == true and move_pos_idx = -1; that means we are in our own goal
   Serial.printf("Ending of Simulation, use_scnd_bins=%d, movePosIdx=%d\n\n", use_scnd_bins, move_pos_idx);
   if (use_scnd_bins == true && move_pos_idx == -1){ // we check -1 because we did set it to -1 because we add to move_idx in next loop
    return board; // this should just return the board state that matches the one given with same player 
   }
   // else then its the next players turn
   else{
    board.playerToMove = other_player;
    return board;
   }
  }



  

// EVENT HANDLERS
  /**
   * event_hander_test_btn: This will start testing code
   */
  static void event_handler_test_btn(lv_event_t *e){
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED){
      mancala_board new_board;
      for (ti=0; ti<6; ti++){
        new_board.player_A_board_bins[ti] = 4;
        new_board.player_B_board_bins[ti] = 4;
      }
      new_board.player_B_goal = 0;
      new_board.player_A_goal = 0;
      new_board.playerToMove = 'A';
      Serial.printf("First Board\n");
      printMancalaBoard(new_board);
      Serial.printf("\nSimulating Move\n");
      mancala_board updated_board = simulate_mancala_move(new_board, 3);
      updated_board = simulate_mancala_move(updated_board, 4);
      printMancalaBoard(updated_board);
    }
  }

/**
 * Requirements:
 *  - GUI should not show moves that have starting bins of zero
 */
void lv_create_main_gui(void) {
  // Disable scrolling on the screen
    lv_obj_clear_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);


  // Create a text label aligned center on top ("Hello, world!")
    lv_obj_t * welcome_label = lv_label_create(lv_screen_active());
    lv_label_set_long_mode(welcome_label, LV_LABEL_LONG_WRAP);    // Breaks the long lines
    lv_label_set_text_fmt(welcome_label, "Lvgl Text");
    lv_obj_set_width(welcome_label, 200);    // Set smaller width to make the lines wrap
    lv_obj_set_style_text_align(welcome_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(welcome_label, LV_ALIGN_TOP_MID, -80, 0);
  
  // Create Debug Label
    debug_label = lv_label_create(lv_screen_active());
    lv_obj_align(debug_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(debug_label, "...");
  
  
  lv_obj_t *btn_label;
  // Create a button for help testing
    lv_obj_t *test_btn = lv_button_create(lv_screen_active());
    lv_obj_add_event_cb(test_btn, event_handler_test_btn, LV_EVENT_ALL, NULL);
    lv_obj_align(test_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_remove_flag(test_btn, LV_OBJ_FLAG_PRESS_LOCK);

    btn_label = lv_label_create(test_btn);
    lv_label_set_text(btn_label, "Test Actions");
    lv_obj_center(btn_label);
}




void setup() {
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);

  // Define my variables
    ti = 0;
  
  // Start LVGL
    lv_init();

  // Register print function for debugging
    lv_log_register_print_cb(log_print);

  // Start the SPI for the touchscreen and init the touchscreen
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);

  // Set the Touchscreen rotation in landscape mode
  // Note: in some displays, the touchscreen might be upside down, so you might need to set the rotation to 0: touchscreen.setRotation(0);
    touchscreen.setRotation(2);

  // Create a display object
    lv_display_t * disp;


  // Initialize the TFT display using the TFT_eSPI library
    disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
    
  // Initialize an LVGL input device object (Touchscreen)
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);

  // Set the callback function to read Touchscreen input
    lv_indev_set_read_cb(indev, touchscreen_read);

  // Function to draw the GUI (text, buttons and sliders)
    lv_create_main_gui();
  }


void loop() {
  lv_task_handler();  // let the GUI do its work
  lv_tick_inc(5);     // tell LVGL how much time has passed
  delay(5);           // let this time pass
}