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

// Manacala Struct
  typedef struct mancala_board {
    char playerToMove; // its the player to take the next move 
    uint8_t player_A_goal;
    uint8_t player_A_board_bins[6];
    
    uint8_t player_B_goal;
    uint8_t player_B_board_bins[6];
    
    // store the (in heap) resulting mancala boards from each possible move from PlayerToMove
    mancala_board *resulting_mancala_boards[6];
  } mancala_board;

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;

// My Global Variables
  mancala_board *gCurrent_mancala_board;
  lv_obj_t *gArr_of_bin_buttons[6]; // this will hold the LVGL buttons for the tab zero

// My LVGL Objects
  static lv_obj_t *debug_label;
  static lv_obj_t *label_playerToMove;
  static lv_obj_t *tabview;


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
   * simulateMancalaMove(board): This function will be given a manacala board state,
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
  mancala_board simulateMancalaMove(mancala_board board, int8_t move_pos_idx){
    // Error Check: if move_pos_idx is <0 or >5 then report error
    if (move_pos_idx < 0 || move_pos_idx > 5){
      Serial.printf("!Bug! (%u)movePosIdx<0||>5\n", move_pos_idx);
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
    if (use_scnd_bins == true && move_pos_idx == -1){ // we check -1 because we did set it to -1 because we add to move_idx in next loop
      return board; // this should just return the board state that matches the one given with same player 
    }
    // else then its the next players turn
    else{
      board.playerToMove = other_player;
      return board;
    }
  }

  /**
   * createInitBoard(): This function will
   * malloc the initial mancala board and return its pointer
   * 
   * Returns:
   *  mancala_board*: The pointer to the malloc initial board
   */
  mancala_board *createInitBoard(){
    mancala_board *init_board = (mancala_board*) malloc(sizeof(mancala_board));
    if (init_board == NULL){
      Serial.printf("!bug! MallocErrorInitBoard");
      debug_msg(debug_label, "!bug! MallocErrorInit");
    }
    for (int i=0; i<6; i++){
      init_board->player_A_board_bins[i] = 4;
      init_board->player_B_board_bins[i] = 4;
    }
    init_board->player_B_goal = 0;
    init_board->player_A_goal = 0;
    init_board->playerToMove = 'A'; 
    return init_board;
  }
  /**
   * heapCopyOfMancalaBoard(board): Given a mancala board, create a copy of it 
   * within the heap and return its pointer.
   * 
   * Parameters:
   *  board (manacala_board): A copy of a mancala board that is to be copied to heap space
   * 
   * Returns:
   *  mancala_board*: A pointer to a (in-heap) stored copy of the board given
   */
  mancala_board *heapCopyOfMancalaBoard(mancala_board board){
    mancala_board *board_copy = (mancala_board*) malloc(sizeof(mancala_board));
    if (board_copy == NULL){ // if Malloc error, report error
      Serial.printf("!bug! heapCopy Malloc Error\n");
      debug_msg(debug_label, "!bug! heapCopy Malloc Error");
      return NULL;
    }
    board_copy->playerToMove = board.playerToMove;
    board_copy->player_A_goal = board.player_A_goal;
    board_copy->player_B_goal = board.player_B_goal;
    for(int i=0; i<6; i++){
      board_copy->player_A_board_bins[i] = board.player_A_board_bins[i];
      board_copy->player_B_board_bins[i] = board.player_B_board_bins[i];
      board_copy->resulting_mancala_boards[i] = board.resulting_mancala_boards[i];
    }
    return board_copy;
  }
  /**
   * fillOutChildrenForBoard(*board): Given a pointer to a mancala board, this function
   * will loop over the bins for the board.playerToMove and for each bin it will check
   * if the bin has marbles and if so it will simulate the player choosing that bin for a move, 
   * and save that board state in an array of children of the board given.
   * 
   * Parameters:
   *  board (mancala_board*): A pointer to a mancala board that is requested to get its children filled out for 
   * 
   * Returns: 
   *  int: 0 if all is well, -1 if error
   */
  int fillOutChildrenForBoard(mancala_board *board){
    uint8_t *first_bins;
    if (board->playerToMove == 'A'){
      first_bins = board->player_A_board_bins;
    } else if (board->playerToMove == 'B'){
      first_bins = board->player_B_board_bins;
    } else{ // report error if neither players were A or B 
      Serial.printf("!Bug! fillChildren,PlayerInvalid\n");
      debug_msg(debug_label, "!Bug! fillChildren,PlayerInvalid");
      return -1;
    }

    for(int i=0; i<6; i++){
      // if the bin has some marbles, then lets simulate a move from their 
      if (first_bins[i] >= 1){
        mancala_board simulated_board = simulateMancalaMove(*board, i);
        if (simulated_board.playerToMove == 'X'){
          return -1; // because there was a problem simulating 
        }

        board->resulting_mancala_boards[i] = heapCopyOfMancalaBoard(simulated_board);
        if (board->resulting_mancala_boards[i] == NULL){ // if there was a malloc error
          return -1; // because malloc error
        }
      }
      // if the bin does NOT have marbles, store null
      else{
        board->resulting_mancala_boards[i] = NULL;
      }
    }

    return 0;
  }

  /**
   * updateSingleLayerOutcomes(*board): Given the pointer to the current global board
   * this function will generate all new children from this board, then display the player
   * to move, updating the background color to reflect player.
   * Then it will loop over each bin position, and if that bin has marbles if will change
   * the button on the screen show show green for 'can play again after this move' and orange for 
   * 'cannot play again after this move'.
   * Then it will also show the result of the current players goal after taking that move within
   * the text of the button.
   * 
   * Then if that bin doesnt have any marbles then it will disable the button 
   * 
   * Parameters:
   *  current_board (mancala_board*): The pointer to the mancala board that we are baseing out button outcomes off of 
   * 
   * Returns: None, updates GUI buttons
   */
  void updateSingleLayerOutcomes(mancala_board *current_board){
    if (fillOutChildrenForBoard(current_board) == -1){
      return; // because there was a malloc error
    }

    lv_label_set_text_fmt(label_playerToMove, "PlayerToMove: %c", current_board->playerToMove);

    // change the background color to reflect which player it is 
    if (current_board->playerToMove == 'A'){
      lv_obj_set_style_bg_color(tabview, lv_color_hex3(0x3aa), 0); // nice blue
    } else if (current_board->playerToMove == 'B'){
      lv_obj_set_style_bg_color(tabview, lv_color_hex3(0xe9f), 0); // light purple
    } else { // if neither playerToMove is found thats a error
      lv_obj_set_style_bg_color(tabview, lv_color_hex3(0xf23), 0); // red for error
      Serial.printf("!Bug! updateOutcomes,PlayerInvalid\n");
      debug_msg(debug_label, "!Bug! updateOutcomes,PlayerInvalid");
      return;
    }

    // for each bin position, if it has marbles pull out the goal of the player, and if they can play again, set button to display that info 
    lv_obj_t *curr_gui_button;
    lv_obj_t *curr_btn_label;

    for (int i=0; i<6; i++){
      mancala_board *curr_child = current_board->resulting_mancala_boards[i];
      curr_gui_button = gArr_of_bin_buttons[i];
      curr_btn_label = lv_obj_get_child(curr_gui_button, 0);
      // if there were no marbles, set this button to disable
      if (curr_child == NULL){
        lv_obj_add_state(curr_gui_button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(curr_gui_button, lv_color_hex3(0x444), 0);
        lv_label_set_text(curr_btn_label, "..");
        continue;
      }
      // if there were marbels in this bin number for the `current_board` then there exists a simulated child. Display its info
      lv_obj_remove_state(curr_gui_button, LV_STATE_DISABLED); 
      bool player_plays_again = curr_child->playerToMove == current_board->playerToMove;
      uint8_t curr_goal;
      if (current_board->playerToMove == 'A'){
        curr_goal = curr_child->player_A_goal;
      } else {
        curr_goal = curr_child->player_B_goal;
      }
      // display the goal outcome in the button label 
      lv_label_set_text_fmt(curr_btn_label, "%u pts", curr_goal);

      // make the button green if the player can play again
      if (player_plays_again == true){
        lv_obj_set_style_bg_color(curr_gui_button, lv_color_hex3(0x5b5), 0); // nice green
      } else{
        lv_obj_set_style_bg_color(curr_gui_button, lv_color_hex3(0xb95), 0); // orange brownish
      }
    }


  }
  

// EVENT HANDLERS
  /**
   * event_handler_bin_button_0: Handles calling update the board 
   */
  static void event_handler_bin_button(lv_event_t *e){
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED){
      int bin_number = (int) lv_event_get_user_data(e);

      // free all children of the current board besides the zero one
      for (int i=0; i<6; i++){
        // if we arent the bin number choosen then free it 
        if (i != bin_number){
          free(gCurrent_mancala_board->resulting_mancala_boards[i]);
        }
      }
      // set the global board to be the zeroth bin child, then free the old global
      mancala_board *temp_old_board = gCurrent_mancala_board;
      gCurrent_mancala_board = gCurrent_mancala_board->resulting_mancala_boards[bin_number];
      free(temp_old_board);

      // call update board using that zeroth bin child
      updateSingleLayerOutcomes(gCurrent_mancala_board);
    }
  }

/**
 * Requirements:
 *  - GUI should not show moves that have starting bins of zero
 */
void lv_create_main_gui(void) {
  // Disable scrolling on the screen
    lv_obj_clear_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);

  // Creating the Tab view
    tabview = lv_tabview_create(lv_screen_active());
    lv_obj_set_size(tabview, lv_pct(100), lv_pct(100));
    lv_obj_t *tabview_tab_0 = lv_tabview_add_tab(tabview, "Single Layer");
    lv_obj_set_style_bg_color(tabview, lv_color_hex3(0xaaa), 0);
    // Tab View 1 - Single Layer
        // Create a text label aligned center on top ("Hello, world!")
          lv_obj_t * welcome_label = lv_label_create(tabview_tab_0);
          lv_label_set_long_mode(welcome_label, LV_LABEL_LONG_WRAP);    // Breaks the long lines
          lv_label_set_text_fmt(welcome_label, "Lvgl Text");
          lv_obj_set_width(welcome_label, 200);    // Set smaller width to make the lines wrap
          lv_obj_set_style_text_align(welcome_label, LV_TEXT_ALIGN_CENTER, 0);
          lv_obj_align(welcome_label, LV_ALIGN_TOP_MID, -80, 0);
        
        // Create Debug Label
          debug_label = lv_label_create(tabview_tab_0);
          lv_obj_align(debug_label, LV_ALIGN_TOP_MID, 0, 0);
          lv_label_set_text(debug_label, "...");

        // Player to move label
          label_playerToMove = lv_label_create(tabview_tab_0);
          lv_obj_align(label_playerToMove, LV_ALIGN_CENTER, 0, -30);
          lv_label_set_text(label_playerToMove, "PlayerToMove: X");
        
        lv_obj_t *btn_label;
        lv_obj_t *bin_button;
        // Create the row of buttons 
        for (int i=0; i<6; i++){
          bin_button = lv_button_create(tabview_tab_0);
          gArr_of_bin_buttons[i] = bin_button;
          lv_obj_add_event_cb(bin_button, event_handler_bin_button, LV_EVENT_ALL, (void*) i);
          lv_obj_align(bin_button, LV_ALIGN_LEFT_MID, i*50, 0);
          lv_obj_set_size(bin_button, lv_pct(15), LV_SIZE_CONTENT);
          lv_obj_remove_flag(bin_button, LV_OBJ_FLAG_PRESS_LOCK);
          //lv_obj_add_state(bin_button, LV_STATE_DISABLED);

          btn_label = lv_label_create(bin_button);
          lv_label_set_text_fmt(btn_label, "%d fpts", i);
          lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_10, 0);
          lv_obj_center(btn_label);
        }
      }




void setup() {
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);

  // Define my variables
    gCurrent_mancala_board = createInitBoard();
  
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

  // Update tab 1 with buttons that show the choices for first pick
    updateSingleLayerOutcomes(gCurrent_mancala_board);
  }


void loop() {
  lv_task_handler();  // let the GUI do its work
  lv_tick_inc(5);     // tell LVGL how much time has passed
  delay(5);           // let this time pass
}