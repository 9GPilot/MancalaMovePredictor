"""
Author: Alex Myers
Purpose: This script will take a output of the Mancala-Move-Predictor of printing a 
    mancala board and it will make a pretty graph.
"""

class mancala_board:
    def __init__(self, A_goal, B_goal, A_bins, B_bins, playerToMove, str_of_children):
        self._A_goal: int          = A_goal
        self._B_goal: int          = B_goal
        self._A_bins: list[int]    = A_bins
        self._B_bins: list[int]    = B_bins
        self._playerToMove: str    = playerToMove
        self._resultingBoards: str = str_of_children # Stores the unprocessesed string including C[Board:...]

def dataStrToObj(dataStr: str) -> mancala_board:
    """
    This function will convert the data string into the board, just dycyphering the data
    """
    dataStr = dataStr[dataStr.find(";")+1:] # get rid of "Board;"
    this_boards_info = dataStr[:dataStr.find('C')-1].split(",")

    playerToMove: str = this_boards_info[0] # single length string 
    A_goal: int = int(this_boards_info[1])
    A_bins: list[str] = this_boards_info[2].split(".")
    B_goal: int = int(this_boards_info[3])
    B_bins: list[str] = this_boards_info[4].split(".")

    str_of_children = dataStr[dataStr.find('C'):]

    return mancala_board( A_goal, B_goal, A_bins, B_bins, playerToMove, str_of_children)

   
def main():
    master_str = "Board;A,0,4.4.4.4.4.4,0,4.4.4.4.4.4,C[Board;B,5,1.4.1.10.1.1,0,3.0.2.9.2.9,C[NULL:NULL:NULL:NULL:NULL:NULL]:Board;A,4,7.1.2.1.8.3,0,7.1.7.0.7.0,C[NULL:NULL:NULL:NULL:NULL:NULL]:Board;A,1,4.4.0.5.5.5,0,4.4.4.4.4.4,C[NULL:NULL:NULL:NULL:NULL:NULL]:Board;B,6,9.2.9.1.4.1,0,1.1.9.3.0.2,C[NULL:NULL:NULL:NULL:NULL:NULL]:Board;A,3,1.6.6.6.0.1,0,6.1.6.6.0.6,C[NULL:NULL:NULL:NULL:NULL:NULL]:Board;A,2,5.0.5.5.5.1,0,5.5.0.5.5.5,C[NULL:NULL:NULL:NULL:NULL:NULL]]"
    mancala_board = dataStrToObj(master_str)



main()