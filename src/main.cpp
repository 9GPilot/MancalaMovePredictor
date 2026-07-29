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
  lv_obj_t *gArr_of_max_goal_buttons[6];
  char gTraceOfMoves[40]; // 40 to play it save
  uint8_t gCurrMoveNumber;
  uint8_t gMaxDepth;

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
  void printMancalaBoard(mancala_board *board, int depth){
    Serial.printf("\n");
    for (int i=0; i<depth; i++){
      Serial.printf("\t");
    }
    if (board == NULL){
      Serial.printf("NULL");
      return;
    }

    Serial.printf("Board;%c,%u,", board->playerToMove, board->player_A_goal);
    for (int i=0; i<6; i++){
      Serial.printf("%u", board->player_A_board_bins[i]);
      if (i<5){
        Serial.printf(".");
      }
    } 
    Serial.printf(",%u,", board->player_B_goal);
    for (int i=0; i<6; i++){
      Serial.printf("%u", board->player_B_board_bins[i]);
      if (i<5){
        Serial.printf(".");
      }
    } 
    Serial.printf(",C[");
    for(int i=0; i<6; i++){
      
      printMancalaBoard(board->resulting_mancala_boards[i], depth+1);
      if (i<5){
        Serial.printf(":");
      }
    }
    Serial.printf("]");
  }

  void debug_msg(lv_obj_t *debug_label, const char* text){
    Serial.printf("Trace of moves = %s\n", gTraceOfMoves);
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
      init_board->resulting_mancala_boards[i] = NULL; // this denotes that the children are not filled out
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
   * This function also checks that the children are not already filled out before filling them out.
   * Its okay for other functions to call this function on boards that already have their children filled out 
   * 
   * This function will set the children of the new children to nulls 
   * 
   * Parameters:
   *  board (mancala_board*): A pointer to a mancala board that is requested to get its children filled out for 
   * 
   * Returns: 
   *  int: 0 if all is well, -1 if error, -2 children already filled out 
   */
  int fillOutChildrenForBoard(mancala_board *board){
    // if the boards children are already filled out then dont fill out the children. We must confirm that all dont equal null because we set a child as null if the bin is zero .I should get this 'bug' on the second turn becaause dfs fills out children. Not a bug because DFS already filled it out before SingleLayerOutcomes changed to child"); // A function might call fillOutChildrenForBoard if its not filled out as to have only one place in the code for this type of check 
    for (int i=0; i<6; i++){
      if (board->resulting_mancala_boards[i] != NULL){
        Serial.printf("!Bug! FillOutChildren,childrenAlreadyFilledOut\n");
        Serial.printf("INFO: child[%d].player = 'bruh'", i);
        debug_msg(debug_label, "!Bug! childrenAlreadyFilledOut");
        return -2;
      }
    }
    
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

        // before saving simulated_board as a child, set its children to null. We do this because displayMaxGoalOutcomes needs to know if the children are already simulated by display displaySingleLayerOutcomes as not not waist computation 
        for (int i=0; i<6; i++){
          simulated_board.resulting_mancala_boards[i] = NULL;
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
  /**
   * freeEntireMancalaBoard(board*): Given the pointer to a mancala board, this funciton
   * will free the boards children (recursively) and free the board given.
   * 
   * Parameters:
   *  board (mancala_board*): A pointer to the mancala board that we want to free
   */
  void freeEntireMancalaBoard(mancala_board *board){
    // if the board has children, free them as well
    for (int i=0; i<6; i++){
      if (board->resulting_mancala_boards[i] != NULL){ // although we can call free on NULL, if us humans read this this makes more sense
        freeEntireMancalaBoard(board->resulting_mancala_boards[i]);
      }
    }
    free(board);
  }
  /**
   * boardIsGameOver(board*): Given a pointer to a board this function will determine if the 
   * game is over and return True if the game is over. A game is over when at least one side of the 
   * board is completely empty (all zeros in bins).
   * 
   * Parameters:
   *  board (mancala_board*): The pointer to the mancala board we want to test 
   * 
   * Returns:
   *  boolean: True if the mancala board has at least one side of its bins as empty
   */
  bool boardIsGameOver(mancala_board *board){
    int A_bin_zero_count = 0;
    int B_bin_zero_count = 0;
    for (int i=0; i<6; i++){
      if (board->player_A_board_bins[i] == 0){
        A_bin_zero_count += 1;
      }
      if (board->player_B_board_bins[i] == 0){
        B_bin_zero_count += 1;
      }
    }
    return A_bin_zero_count == 6 || B_bin_zero_count == 6;
  }
  /**
   * getPtrToPlayersBins(board*, playerOfInterest): This function will be given a board, and a player 
   * of interest and it will return the pointer to the bins of the player of interest.
   * 
   * Parameters:
   *  board (mancala_board*): A pointer to a mancala board
   *  playerOfInterest (char): A single character ('A' or 'B') representing the player of interest
   * 
   * Returns:
   *  uint8_t*: The pointer to the bins in the given board that corrilate to the playerOfInterest bins
   */
  uint8_t *getPtrToPlayersBins(mancala_board *board, char playerOfInterest){
    if (board->playerToMove == 'A'){
      return board->player_A_board_bins;
    } else if (board->playerToMove == 'B'){
      return board->player_B_board_bins;
    } else{ // report error if neither players were A or B 
      Serial.printf("!Bug! getPlayersBins,PlayerInvalid\n");
      debug_msg(debug_label, "!Bug! getPlayersBins,PlayerInvalid");
      return NULL;
    }
  }
  /**
   * findMaxGoalAchievable(board*, playerOfInterest, depth): Given a board, find the maximum achievable
   * goal of the player of interest given, while only search the depth given. If depth is zero, that means
   * return the currrent boards goal for the player of interest, if its one, then return the maximum of the children
   * of this boards.
   * 
   * This function will loop over each each of the bins and find its max goal achievalbe for the playerOfInterest
   * and then this function compares all childrens goals to see which has the best, then this function returns
   * the best it found.
   * 
   * Parameters:
   *  board (mancala_board*): The mancala board we wish to compute the maximum achievable goal for 
   *  playerOfInterest (char): The single letter ('A' or 'B') which denotes the goal we want to find the max of
   *  depth (int): The amount of layers down we want to search and find 
   * 
   * Returns:
   *  int: the best maximum acheivable goal found from the children. -1 if there is an error
   */
  int findMaxGoalAchievable(mancala_board *board, char playerOfInterest, int depth){
    // if the board given is null, that is an error
    if (board == NULL){
      Serial.printf("!bug! maxAchievable Board=NuLL");
      debug_msg(debug_label, "!bug! maxAchievable Board=NuLL");
      return -1;
    }
    // if depth == 0 return the current goal
    // if the current board given is game over then return the playerOfInterestsGoal
    if (depth == 0 || boardIsGameOver(board) == true){
        if (playerOfInterest == 'A'){
          return board->player_A_goal;
        } else if (playerOfInterest == 'B'){
          return board->player_B_goal;
        } else{ // report error if neither players were A or B 
          Serial.printf("!Bug! maxAchievoable,PlayerInvalid\n");
          debug_msg(debug_label, "!Bug! maxAchievalbe,PlayerInvalid");
          return -1;
      }
    }
    // lets fill out the children for this board, it is expected that there isnt already children 
    int result = fillOutChildrenForBoard(board);
    if (result == -1){
      return -1; // malloc error
    }
    // Note: its fine for there to be children because that means we are looking just 1 layer down and the updateSingleLayer func already filled them out for us 
    // determine the bins of the player to move
    uint8_t *playersToMove_bins = getPtrToPlayersBins(board, board->playerToMove);
    if (playersToMove_bins == NULL){
      Serial.printf("!bug! invalidPlayerfrom,maxAchievable\n");
      return -1;
    }
    // loop over all the bins, if there are marbles, then recursive call to find the max of that child
    int best_max = -2;
    for (int i=0; i<6; i++){
      // if this bin has marbles
      if (playersToMove_bins[i] >= 1){
        // find the max goal if we choose this bin to move
        int child_max = findMaxGoalAchievable(board->resulting_mancala_boards[i], playerOfInterest, depth-1);
        freeEntireMancalaBoard(board->resulting_mancala_boards[i]);
        board->resulting_mancala_boards[i] = NULL;
        if (child_max == -1){ // if there was any type of error, keep passing it up to effectivley cancel this run
          return -1;
        }
        if (best_max == -2 || child_max > best_max){
          best_max = child_max;
        }
      }
    }
    return best_max;
  }
  /**
   * displayMaxGoalOutcomes(current_board, depth): This function will be given 
   * the current board, and it will go over each bin for the current player and calculate the maximum
   * possible goal amount achieved for the depth given.
   * This function will call on another function that searches for the maximum possible goal acheieved 
   * for the given player then compairs the results.
   * 
   * Note 1A:
   *  This function requires that the children get filled out before this function 
   *  is called because otherwise if we fill  them out we should free them, but we 
   *  cannot free them if someone else fills them out because what if UpdateSingleLayerOutcomes 
   *  fills them out and the user presses on the button to chose that outcome then there is no child!
   * 
   * Its important to note, the larger the depth the LESS useful the information is because it will show 
   * similar max values because the game is over. Its more useful for shorter bursts of moves.
   * 
   * Parameters:
   *  current_board (mancala_board*): Pointer to the current board displayed on the screen
   *  depth (int): The max depth to search for the max, the larger the more computation 
   * 
   */
  void displayMaxGoalOutcomes(mancala_board *current_board, int depth){
    // we require that the children get filled out before this function. See NOTE 1A in docstring
    bool good_to_go_there_is_children = false;
    for (int i=0; i<6; i++){
      // if we find a child then we are good to go! ✔
      if (current_board->resulting_mancala_boards[i] != NULL){
        good_to_go_there_is_children = true;
        break;
      }
    }
    if (! good_to_go_there_is_children){
      Serial.printf("!bug! maxGoalOutcomes requires children filled out");
      debug_msg(debug_label, "!bug! maxGoalOutcome requiresChildren");
      return;
    }
    
    // determine the bins we are looping over 
    uint8_t *curr_player_bins;
    if (current_board->playerToMove == 'A'){
      curr_player_bins = current_board->player_A_board_bins;
    } else if (current_board->playerToMove == 'B'){
      curr_player_bins = current_board->player_B_board_bins;
    } else{ // report error if neither players were A or B 
      Serial.printf("!Bug! maxOutComes,PlayerInvalid\n");
      debug_msg(debug_label, "!Bug! maxOutComes,PlayerInvalid");
      return;
    }

    // loop over all the bins, then find the maximum acheviable goal for that bin and display it
    for (int i=0; i<6; i++){
      lv_obj_t *curr_label_maxGoal = lv_obj_get_child(gArr_of_max_goal_buttons[i], 0);
      // if there are marbles in the bins then find the max of it (and that should mean there is a resulting board for this bin already, NOTE 1A)
      if (curr_player_bins[i] >= 1){ 
        // if by some mericle there isnt a child for this index, thats a bug, NOTE 1A
        if (current_board->resulting_mancala_boards[i] == NULL){
          Serial.printf("!bug! dispMaxOutcomes, expectedAChildIfMarbles>=1\n");
          debug_msg(debug_label, "!bug! dispMaxOutcomes, expectedAChildIfMarbles>=1");
          return;
        }

        int child_max = findMaxGoalAchievable(current_board->resulting_mancala_boards[i], current_board->playerToMove, depth);
        if (child_max == -1){ // if MALLOC Error
          return;
        }

        lv_label_set_text_fmt(curr_label_maxGoal, "%u", child_max);
      }
      // if the bin has no marbles then no point of showing max because they cannot player there
      else{
        lv_label_set_text(curr_label_maxGoal, "...");
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

      gTraceOfMoves[gCurrMoveNumber] = (char) bin_number + '0';
      gCurrMoveNumber += 1;

      // call update board using that zeroth bin child
      updateSingleLayerOutcomes(gCurrent_mancala_board);
      displayMaxGoalOutcomes(gCurrent_mancala_board, gMaxDepth);
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
        
        // Create Debug Label
          debug_label = lv_label_create(tabview_tab_0);
          lv_obj_align(debug_label, LV_ALIGN_TOP_MID, 0, 0);
          lv_label_set_text(debug_label, "...");

        // Player to move label
          label_playerToMove = lv_label_create(tabview_tab_0);
          lv_obj_align(label_playerToMove, LV_ALIGN_CENTER, 0, -30);
          lv_label_set_text(label_playerToMove, "PlayerToMove: X");

        // Max Goal Achievable Label
          lv_obj_t *label_maxGoalAchievable = lv_label_create(tabview_tab_0);
          lv_obj_align(label_maxGoalAchievable, LV_ALIGN_CENTER, 0, 30);
          lv_label_set_text_fmt(label_maxGoalAchievable, "Max Achievable. D=%u", gMaxDepth);
        
        lv_obj_t *btn_label;
        lv_obj_t *bin_button;
        lv_obj_t *maxGoal_button;
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

          maxGoal_button = lv_button_create(tabview_tab_0);
          gArr_of_max_goal_buttons[i] = maxGoal_button;
          lv_obj_align(maxGoal_button, LV_ALIGN_LEFT_MID, i*50, 60);
          lv_obj_set_size(maxGoal_button, lv_pct(15), LV_SIZE_CONTENT);
          lv_obj_remove_flag(maxGoal_button, LV_OBJ_FLAG_CLICKABLE);
          lv_obj_set_style_bg_color(maxGoal_button, lv_color_hex(0xf59e0b), 0);
          lv_obj_set_style_radius(maxGoal_button, 24, 0);
          lv_obj_set_style_border_color(maxGoal_button, lv_color_hex(0x78350f), 0);
          lv_obj_set_style_border_width(maxGoal_button, 2, 0);

          btn_label = lv_label_create(maxGoal_button);
          lv_label_set_text(btn_label, "d max");
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
    gCurrMoveNumber = 0;
    gMaxDepth = 2;
  
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
    displayMaxGoalOutcomes(gCurrent_mancala_board, gMaxDepth);
  }


void loop() {
  lv_task_handler();  // let the GUI do its work
  lv_tick_inc(5);     // tell LVGL how much time has passed
  delay(5);           // let this time pass
}