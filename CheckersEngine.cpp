#include <algorithm>
#include <iostream>
#include <cstdint>
#include <array>
#include <vector>
#include <bit>
#include <bitset>
#include <limits>

constexpr auto BITBOARD_SIZE = 31;

// We now use a 32‐bit bitboard to represent the 32 playable squares.

using Bitboard = uint32_t;
enum MoveType
{
    URCapture,
    ULCapture,
    DRCapture,
    DLCapture,
    URMove,
    ULMove,
    DRMove,
    DLMove,
};


struct Move {
    uint8_t from;
    uint8_t to;

    Move(uint8_t f,uint8_t t):from(f),to(t){}
};


std::ostream & operator << (std::ostream & outs, const Move & move) {
    return outs <<"from:"<< move.from << " to:" << move.to;
}

struct GameState {
	Bitboard white;
	Bitboard whiteMask;
	Bitboard black;
	Bitboard blackMask;
	Bitboard kings;
	Bitboard kingsMask;
    Bitboard empty;

	GameState(
		Bitboard w,
		Bitboard wMask,
		Bitboard b,
		Bitboard bMask,
		Bitboard k,
		Bitboard kMask,
		Bitboard e 
	) :white(w), whiteMask(wMask), black(b), blackMask(bMask), kings(k), kingsMask(kMask),empty(e) {
	}
};


std::ostream & operator << (std::ostream & outs, const Move & move) {
    return outs <<"from:"<< move.from << " to:" << move.to;
}
// Initialize the move arrays. Our mapping is based on the idea that
// the 32 playable squares come from an 8x8 board where:
//   - Rows are numbered 0 (bottom) to 7 (top)
//   - Each row has 4 playable squares.
//   - On even rows (0, 2, 4, 6), playable squares are in columns 0,2,4,6.
//   - On odd rows (1, 3, 5, 7), playable squares are in columns 1,3,5,7.
struct MoveArrays {
    std::array<Bitboard, 32> whiteManLeft;
    std::array<Bitboard, 32> whiteManRight;
    std::array<Bitboard, 32> whiteManCapturesLeft;
    std::array<Bitboard, 32> whiteManCapturesRight;
    std::array<Bitboard, 32> blackManLeft;
    std::array<Bitboard, 32> blackManRight;
    std::array<Bitboard, 32> blackManCapturesLeft;
    std::array<Bitboard, 32> blackManCapturesRight;
    std::array<Bitboard, 32> King;
};


//--------------------------------------------------------------------
// Utility: initialize the moves array for the white/black man and kings.
//--------------------------------------------------------------------

consteval MoveArrays initMoveArrays() {
    MoveArrays moves{};  // Value-initialize all arrays
    constexpr bool LEFT_EDGE[32] = {
        true,false,false,false,
        true,false,false,false,
        true,false,false,false,
        true,false,false,false,
        true,false,false,false,
        true,false,false,false,
        true,false,false,false,
        true,false,false,false,
    }; constexpr bool RIGHT_EDGE[32] = {
        false,false,false,true,
        false,false,false,true,
        false,false,false,true,
        false,false,false,true,
        false,false,false,true,
        false,false,false,true,
        false,false,false,true,
        false,false,false,true,
    };

    constexpr bool EDGES[32] = {
        true,false,false,true,
        true,false,false,false,
        false,false,false,true,
        true,false,false,false,
        false,false,false,true,
        true,false,false,false,
        false,false,false,true,
        true,false,false,false,
    };
    for (int i = 0; i < 32; i++)
    {
        //setting up the white  man pieces by shifting them down left/right =>  and +3/4   
        if (i<28)
        {
            if (EDGES[i])
            {
            moves.whiteManLeft[i] |= 1u << (i + 4);
            moves.whiteManCapturesLeft[i] |= 1u << (i + 4);
            }
            else {
            moves.whiteManRight[i] |= 1u << (i + 5);
            moves.whiteManLeft[i] |= 1u << (i + 4);
            }
            if (i+9<32)
            {
				if (!RIGHT_EDGE[i])
                {
                    moves.whiteManCapturesRight[i] |= 1u << (i + 9);

                }
				if (!LEFT_EDGE[i])
                {
                    moves.whiteManCapturesLeft[i] |= 1u << (i + 7);
                }
            }

        }
        else {
            moves.whiteManRight[i] = 0;
            moves.whiteManLeft[i] = 0;
            moves.whiteManCapturesRight[i] = 0;
            moves.whiteManCapturesLeft[i] = 0;
        }
        //setting up the black man pieces by shifting them down left/right => -3/4 
        if (i>3)
        {
            if (i== 4 ||i== 12 ||i== 20 ||i== 28 ||i== 11 ||i== 19 ||i== 27)
            {
            moves.blackManRight[i] |= 1u << (i - 4);
            }
            else {
            moves.blackManLeft[i] |= 1u << (i - 5);
            moves.blackManRight[i] |= 1u << (i - 4);
            if (!LEFT_EDGE[i])
            {
				if (i - 8 >= 0)
				{
					moves.blackManCapturesLeft[i] |= 1u << (i - 8);
				}
            }
            if (!RIGHT_EDGE[i])
            {
				if (i - 6 >= 0)
				{
					moves.blackManCapturesRight[i] |= 1u << (i - 6);
				}
            }
            }

        }
        else {
            moves.blackManLeft[i] = 0;
            moves.blackManRight[i] = 0;
            moves.blackManCapturesLeft[i] = 0;
            moves.blackManCapturesRight[i] = 0;
        }
        }
    return moves;
}

constexpr auto moves_array = initMoveArrays();

//this are the masks for the active pieces ie the pieces that can make a legal move 
//this is the last row for white and last for black
Bitboard whiteActive = 0x00000F00u;
Bitboard blackActive = 0x00F00000u;

//--------------------------------------------------------------------
// Utility: Print the 32-bit board as an 8x8 grid.
// Only the 32 playable squares are represented by a symbol ('1' if the
// corresponding bit is set, '.' if not). Non-playable squares are left blank.
//--------------------------------------------------------------------

// Prints an 8x8 board. Dark squares show the corresponding bit (as '1' if set, '.' if not).
// Light squares are printed as '.'.
void printBitBoard(uint32_t board) {
    int bitIndex = 0;  // Index into the 32-bit board bits.
    
    // Loop over 8 rows.
    for (int row = 0; row < 8; row++) {
        // Loop over 8 columns.
        for (int col = 0; col < 8; col++) {
            // Determine if this square is dark.
            // On even rows (0,2,4,6): dark squares are at odd columns.
            // On odd rows (1,3,5,7): dark squares are at even columns.
            bool isDark = (row % 2 == 0) ? (col % 2 == 1) : (col % 2 == 0);
            
            if (isDark) {
                // For a dark square, check the corresponding bit.
                char cell = (board & (1u << bitIndex)) ? '1' : '.';
                std::cout << cell;
                bitIndex++;  // Advance to the next bit in our 32-bit board.
            } else {
                // For light squares, print a dot (or you can choose a space).
                std::cout << '.';
            }
            
            // Print a space between columns (optional).
            if (col < 7) std::cout << " ";
        }
        std::cout << std::endl;
    }
}

inline static bool isURcapture(Bitboard board, Bitboard empty) {
    //(empty& (1u >> 4)) ==0  
    if ((empty& (1u >> 4)) ==0)
    {
        if ((board & empty) !=0)
        {
            return MoveType::URCapture;
        }
    }
    return MoveType::URMove;
}

inline static bool isULcapture(Bitboard board, Bitboard empty) {
    //(empty& (1u >> 4)) ==0  
    if ((empty& (1u >> 4)) ==0)
    {
        if ((board & empty) !=0)
        {
            return true;
        }
    }
    return false;
}

inline static bool isDRcapture(Bitboard board, Bitboard empty) {
    //(empty& (1u >> 4)) ==0  
    if ((empty& (1u >> 4)) ==0)
    {
        if ((board & empty) !=0)
        {
            return true;
        }
    }
    return false;
}

inline static bool isDLcapture(Bitboard board, Bitboard empty) {
    //(empty& (1u >> 4)) ==0  
    if ((empty& (1u >> 4)) ==0)
    {
        if ((board & empty) !=0)
        {
            return true;
        }
    }
    return false;
}


using MaskFunction = void(*)(uint8_t from, Bitboard* mask,Bitboard* board,Bitboard* active);

inline static void MaskURCapture(uint8_t from,Bitboard* mask,Bitboard* board,Bitboard* active) {
		//this clear the bit
		*mask &= ~(1UL << from);
		//this sets the bit before the current bit
		*mask |= ~(1UL << from + 3);
		*mask |= ~(1UL << from + 4);
        //this sets the new jump bit
		*mask |= ~(1UL << from - 7);
        *active = *board & *mask;
}

inline static void MaskULCapture(uint8_t from,Bitboard* mask,Bitboard* board,Bitboard* active) {
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit before the current bit
		*board |= ~(1UL << from + 3);
		*board |= ~(1UL << from + 4);
        //this sets the new jump bit
		*board |= ~(1UL << from - 9);

}

inline static void MaskDRCapture(uint8_t from,Bitboard* mask,Bitboard* board,Bitboard* active) {
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit before the current bit
		*board |= ~(1UL << from - 4);
		*board |= ~(1UL << from - 5);
        //this sets the new jump bit
		*board |= ~(1UL << from + 9);
}

inline static void MaskDLCapture(uint8_t from,Bitboard* mask,Bitboard* board,Bitboard* active) {
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit before the current bit
		*board |= ~(1UL << from - 4);
		*board |= ~(1UL << from - 5);
        //this sets the new jump bit
		*board |= ~(1UL << from + 7);
}

inline static void MaskULMove(uint8_t from,Bitboard* mask,Bitboard* board,Bitboard* active) {
		//this clear the bit
		*board &= ~(1UL << from);
        //this sets the new jump bit
		*board |= ~(1UL << from - 4);
}
inline static void MaskURMove(uint8_t from,Bitboard* mask,Bitboard* board,Bitboard* active) {
		//this clear the bit
		*board &= ~(1UL << from);
        //this sets the new jump bit
		*board |= ~(1UL << from - 3);
}

inline static void MaskDLMove(uint8_t from,Bitboard* mask,Bitboard* board,Bitboard* active) {
		//this clear the bit
		*board &= ~(1UL << from);
        //this sets the new jump bit
		*board |= ~(1UL << from + 3);
}

inline static void MaskDRMove(uint8_t from,Bitboard* mask,Bitboard* board,Bitboard* active) {
		//this clear the bit
		*board &= ~(1UL << from);
        //this sets the new jump bit
		*board |= ~(1UL << from + 4);
}


// Create lookup table
static const MaskFunction mask_functions[] = {
    &MaskURCapture,
    &MaskULCapture,
    &MaskDRCapture,
    &MaskDLCapture,
    &MaskURMove,
    &MaskULMove,
    &MaskDRMove,
    &MaskDLMove,
};

using MoveFunction = void(*)(uint8_t from, Bitboard* white,Bitboard* black);


inline static void MakeURCaptureWhite(uint8_t from, Bitboard* white,Bitboard* black) {
		//this clear the bit
		*white &= ~(1UL << from);
        //clears the captured pieces's bit
		*black &= (1UL << (from - 6));
		//this clear the bit between the jump
		*white &= ~(1UL << from - 3);
		//this sets the bit
		*white |= (1UL << (from - 6));
}

inline static void MakeULCaptureWhite(uint8_t from, Bitboard* white,Bitboard* black) {
		//this clear the bit
		*white &= ~(1UL << from);
        //clears the captured pieces's bit
		*black &= (1UL << (from - 8));
		//this clear the bit between the jump
		*white &= ~(1UL << from - 4);
		//this sets the bit
		*white |= (1UL << (from - 8));

}

inline static void MakeDRCaptureWhite(uint8_t from, Bitboard* white,Bitboard* black) {
		//this clear the bit
		*white &= ~(1UL << from);
        //clears the captured pieces's bit
		*black &= (1UL << (from + 9));
		//this clear the bit between the jump
		*white &= ~(1UL << from + 5);
		//this sets the bit
		*white |= (1UL << (from + 9));
}

inline static void MakeDLCaptureWhite(uint8_t from, Bitboard* white,Bitboard* black) {
		//this clear the bit
		*white &= ~(1UL << from);
        //clears the captured pieces's bit
		*black &= (1UL << (from + 7));
		//this clear the bit between the jump
		*white &= ~(1UL << from + 4);
		//this sets the bit
		*white |= (1UL << (from + 7));
}

inline static void MakeURCaptureBlack(uint8_t from,Bitboard* black,Bitboard* white) {
		//this clear the bit
		*black &= ~(1UL << from);
        //clears the captured pieces's bit
		*white &= (1UL << (from - 6));
		//this clear the bit between the jump
		*black &= ~(1UL << from - 3);
		//this sets the bit
		*black |= (1UL << (from - 6));
}

inline static void MakeULCaptureBlack(uint8_t from,Bitboard* black,Bitboard* white) {
		//this clear the bit
		*black &= ~(1UL << from);
        //clears the captured pieces's bit
		*white &= (1UL << (from - 8));
		//this clear the bit between the jump
		*black &= ~(1UL << from - 4);
		//this sets the bit
		*black |= (1UL << (from - 8));

}

inline static void MakeDRCaptureBlack(uint8_t from,Bitboard* black,Bitboard* white) {
		//this clear the bit
		*black &= ~(1UL << from);
        //clears the captured pieces's bit
		*white &= (1UL << (from + 9));
		//this clear the bit between the jump
		*black &= ~(1UL << from + 5);
		//this sets the bit
		*black |= (1UL << (from + 9));
}

inline static void MakeDLCaptureBlack(uint8_t from,Bitboard* black,Bitboard* white) {
		//this clear the bit
		*black &= ~(1UL << from);
        //clears the captured pieces's bit
		*white &= (1UL << (from + 7));
		//this clear the bit between the jump
		*black &= ~(1UL << from + 4);
		//this sets the bit
		*black |= (1UL << (from + 7));
}

inline static void MakeULMove(uint8_t from,Bitboard* board,Bitboard* o= nullptr) {
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit
		*board |= 1UL << (from-4);
}
inline static void MakeURMove(uint8_t from,Bitboard* board,Bitboard* o= nullptr) {
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit
		*board |= 1UL << (from-3);
}

inline static void MakeDLMove(uint8_t from,Bitboard* board,Bitboard* o= nullptr) {
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit
		*board |= 1UL << (from+3);
}

inline static void MakeDRMove(uint8_t from,Bitboard* board,Bitboard* o= nullptr) {
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit
		*board |= 1UL << (from+4);
}


// Create lookup table
static const MoveFunction move_functions_white[] = {
    &MakeURCaptureWhite,
    &MakeULCaptureWhite,
    &MakeDRCaptureWhite,
    &MakeDLCaptureWhite,
    &MakeURMove,
    &MakeULMove,
    &MakeDRMove,
    &MakeDLMove,
};
static const MoveFunction move_functions_black[] = {
    &MakeURCaptureBlack,
    &MakeULCaptureBlack,
    &MakeDRCaptureBlack,
    &MakeDLCaptureBlack,
    &MakeURMove,
    &MakeULMove,
    &MakeDRMove,
    &MakeDLMove,
};

inline void MakeMoveWhite(uint8_t from,GameState game_state, MoveType move_type) {
    move_functions_white[static_cast<int>(move_type)](from, &game_state.white,&game_state.black);
    mask_functions[static_cast<int>(move_type)](from, mask,whiteBoard, whiteActive);
    findMoveWhite(whiteBoard, empty);
}


void findMoveWhite(Bitboard *whitePiece,Bitboard* empty){

    std::vector<Move> legalMoves;
    Bitboard ActivePieces = *whitePiece & whiteActive;

    for (int i = std::countr_zero(ActivePieces); i <= BITBOARD_SIZE-std::countl_zero(ActivePieces); i++)
    {
        if (isDLcapture(moves_array.whiteManLeft[i],*empty))
        {
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.whiteManLeft[i])));
            MakeMoveWhite(i,&whiteActive,whitePiece,&ActivePieces,empty,MoveType::DLCapture);
        }

        if (isDRcapture(moves_array.whiteManLeft[i],*empty))
        {
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.whiteManLeft[i])));
            MakeMoveWhite(i,&whiteActive,whitePiece,&ActivePieces,empty,MoveType::DLCapture);
        }
        if ((moves_array.whiteManLeft[i]& *empty) !=0)
        {
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.whiteManLeft[i])));
            MakeMoveWhite(i,&whiteActive,whitePiece,&ActivePieces,empty,MoveType::DLCapture);
        }
        if ((moves_array.whiteManRight[i]& *empty) !=0)
        {
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.whiteManLeft[i])));
            MakeMoveWhite(i,&whiteActive,whitePiece,&ActivePieces,empty,MoveType::DLCapture);
        }

    }
}



void findMoveBlack(Bitboard *blackPiece,Bitboard occupied,Bitboard empty){

    std::vector<Move> legalMoves;
    Bitboard ActivePieces = *blackPiece & blackActive;
    for (int i = std::countr_zero(ActivePieces); i <= BITBOARD_SIZE-std::countl_zero(ActivePieces); i++)
    {
        if (isDLcapture(moves_array.blackManLeft[i],empty))
        {
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.blackManLeft[i])));
            MakeMove(i,&blackActive,blackPiece,&ActivePieces,MoveType::DLCapture);
        }

        if (isDRcapture(moves_array.blackManLeft[i],empty))
        {
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.blackManLeft[i])));
            MakeMove(i,&blackActive,blackPiece,&ActivePieces,MoveType::DLCapture);
        }
        if ((moves_array.blackManLeft[i]&empty) !=0)
        {
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.blackManLeft[i])));
            MakeMove(i,&blackActive,blackPiece,&ActivePieces,MoveType::DLCapture);
        }
        if ((moves_array.blackManRight[i]&empty) !=0)
        {
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.blackManLeft[i])));
            MakeMove(i,&blackActive,blackPiece,&ActivePieces,MoveType::DLCapture);
        }

    }
    
}
// Optimized recursive minimax using the contiguous tree.
int recursiveMinimaxContiguous(int index, int depth, bool isMaximizing) {
  const Node &node = treeNodes[index];
  if (depth == 0)
    return node.value;

  int bestValue;
  if (isMaximizing) {
    bestValue =  std::numeric_limits<int>::min();
    for (int childIndex : node.children)
      bestValue = std::max(bestValue,
                      recursiveMinimaxContiguous(childIndex, depth - 1, false));
  } else {
    bestValue = std::numeric_limits<int>::max();
    for (int childIndex : node.children)
      bestValue = std::min(bestValue,
                      recursiveMinimaxContiguous(childIndex, depth - 1, true));
  }
  return bestValue;
}




//--------------------------------------------------------------------
// Main: Set up the initial position and generate moves.
//--------------------------------------------------------------------
int main() {
    clock_t before = clock();
    // Initial positions:
    // We place red pieces on the bottom three rows (rows 0, 1, 2 → indices 0..11)
    // and black pieces on the top three rows (rows 5,6,7 → indices 20..31).

    Bitboard whitePieces = 0;
    for (int i = 0; i < 12; i++)
        whitePieces |= (1 << i);
    
    Bitboard blackPieces = 0;
    for (int i = 20; i < 32; i++)
        blackPieces |= (1 << i);
    
    Bitboard occupied = whitePieces | blackPieces;
    Bitboard empty= ~occupied&0xFFFFFFFF;
    

    
    std::cout << "Initial White Pieces (32-bit):\n";
    printBitBoard(whitePieces);
    
    std::cout << "Initial Black Pieces (32-bit):\n";
    printBitBoard(blackPieces);

    std::cout << "Initial White Pieces captures (32-bit):\n";
    for (auto c : moves_array.whiteManCapturesLeft) {
        std::cout << std::bitset<32>(c) << std::endl;
    }
    
    std::cout << "Empty Playable Squares:\n";
    // The empty squares are the 32 bits not set in 'occupied'
    printBitBoard(empty);
    /*
    for (size_t i = 0; i < 100000000; i++)
    {
    findMoveWhite(&whitePieces, occupied, empty);
    findMoveBlack(&blackPieces, occupied, empty);
    }*/
    findMoveWhite(&whitePieces, occupied, empty);
    std::cout << "White Board (32-bit):\n";
    printBitBoard(whitePieces );
    std::cout << "Black Board (32-bit):\n";
    printBitBoard(blackPieces);
    std::cout << "Board (32-bit):\n";
    printBitBoard(whitePieces |blackPieces);
    std::cout << "it took" << (float)(clock() - before) / CLOCKS_PER_SEC <<"s"<< std::endl;

    return 0;
}

