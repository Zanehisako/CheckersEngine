#include <iostream>
#include <cstdint>
#include <array>
#include <vector>
#include <bit>
#include <bitset>

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
// Initialize the move arrays. Our mapping is based on the idea that
// the 32 playable squares come from an 8x8 board where:
//   - Rows are numbered 0 (bottom) to 7 (top)
//   - Each row has 4 playable squares.
//   - On even rows (0, 2, 4, 6), playable squares are in columns 0,2,4,6.
//   - On odd rows (1, 3, 5, 7), playable squares are in columns 1,3,5,7.
struct MoveArrays {
    std::array<Bitboard, 32> whiteMan;
    std::array<Bitboard, 32> whiteManCaptures;
    std::array<Bitboard, 32> blackMan;
    std::array<Bitboard, 32> blackManCaptures;
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
            moves.whiteMan[i] |= 1u << (i + 4);
            moves.whiteManCaptures[i] |= 1u << (i + 4);
            }
            else {
            moves.whiteMan[i] |= 1u << (i + 5);
            moves.whiteMan[i] |= 1u << (i + 4);
            }
            if (i+9<32)
            {
				if (!RIGHT_EDGE[i])
                {
                    moves.whiteManCaptures[i] |= 1u << (i + 9);

                }
				if (!LEFT_EDGE[i])
                {
                    moves.whiteManCaptures[i] |= 1u << (i + 7);
                }
            }

        }
        else {
            moves.whiteMan[i] = 0;
            moves.whiteManCaptures[i] = 0;
        }
        //setting up the black man pieces by shifting them down left/right => -3/4 
        if (i>3)
        {
            if (i== 4 ||i== 12 ||i== 20 ||i== 28 ||i== 11 ||i== 19 ||i== 27)
            {
            moves.blackMan[i] |= 1u << (i - 4);
            }
            else {
            moves.blackMan[i] |= 1u << (i - 5);
            moves.blackMan[i] |= 1u << (i - 4);
            if (!LEFT_EDGE[i])
            {
				if (i - 8 >= 0)
				{
					moves.blackManCaptures[i] |= 1u << (i - 8);
				}
            }
            if (!RIGHT_EDGE[i])
            {
				if (i - 6 >= 0)
				{
					moves.blackManCaptures[i] |= 1u << (i - 6);
				}
            }
            }

        }
        else {
            moves.blackMan[i] = 0;
            moves.blackManCaptures[i] = 0;
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
            return MoveType::ULCapture;
        }
    }
    return MoveType::ULMove;
}

inline static bool isDRcapture(Bitboard board, Bitboard empty) {
    //(empty& (1u >> 4)) ==0  
    if ((empty& (1u >> 4)) ==0)
    {
        if ((board & empty) !=0)
        {
            return MoveType::DRCapture;
        }
    }
    return MoveType::DRMove;
}

inline static bool isDLcapture(Bitboard board, Bitboard empty) {
    //(empty& (1u >> 4)) ==0  
    if ((empty& (1u >> 4)) ==0)
    {
        if ((board & empty) !=0)
        {
            return MoveType::DLCapture;
        }
    }
    return MoveType::DLMove;
}


template<typename T>
inline void MakeMove(uint8_t from,Bitboard* board,  T move_type)
{
}

template <>
inline static void MakeMove<MoveType>(uint8_t from,Bitboard* board, MoveType move_type) {

	switch (move_type)
	{
	case URCapture:

		//this clear the bit
		*board &= ~(1UL << from);
		//this clear the bit between the jump
		*board &= ~(1UL << from - 3);
		//this sets the bit
		*board |= (1UL << (from - 6));
		printBitBoard(*board);
		break;

	case ULCapture:
		//this clear the bit
		*board &= ~(1UL << from);
		//this clear the bit between the jump
		*board &= ~(1UL << from - 4);
		//this sets the bit
		*board |= (1UL << (from - 8));
		printBitBoard(*board);
		break;

	case DRCapture:
		//this clear the bit
		*board &= ~(1UL << from);
		//this clear the bit between the jump
		*board &= ~(1UL << from + 5);
		//this sets the bit
		*board |= (1UL << (from + 9));
		printBitBoard(*board);
		break;

	case DLCapture:
		//this clear the bit
		*board &= ~(1UL << from);
		//this clear the bit between the jump
		*board &= ~(1UL << from + 4);
		//this sets the bit
		*board |= (1UL << (from + 7));
		printBitBoard(*board);
		break;

	case URMove:
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit
		*board |= 1UL << (from-3);
		printBitBoard(*board);
		break;

	case ULMove:
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit
		*board |= 1UL << (from-4);
		printBitBoard(*board);
		break;
	case DRMove:
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit
		*board |= 1UL << (from+4);
		printBitBoard(*board);
		break;
	case DLMove:
		//this clear the bit
		*board &= ~(1UL << from);
		//this sets the bit
		*board |= 1UL << (from+3);
		printBitBoard(*board);
		break;
	default:
		break;
	}
}




void findMoveWhite(Bitboard *whitePiece,Bitboard occupied,Bitboard empty){

    std::vector<Move> legalMoves;
    Bitboard ActivePieces = *whitePiece & whiteActive;
/*
    std::cout << "Active pieces:\n";
    printBitBoard(ActivePieces);
    std::cout << "moves at 11:\n" << std::bitset<32>(moves_array.whiteMan[11]) << std::endl;
    std::cout << "moves at 8:\n" << std::bitset<32>(moves_array.whiteMan[8]) << std::endl;
    std::cout << std::bitset<32>(ActivePieces) << std::endl;
    std::cout << std::countr_zero(ActivePieces) << std::endl;
    std::cout << "the index of first 1 starting form most significant bit\n";
    std::cout << std::countl_zero(ActivePieces) << std::endl;
    std::cout << "the index of first 1 starting form least significant bit\n";
*/
    for (int i = std::countr_zero(ActivePieces); i <= BITBOARD_SIZE-std::countl_zero(ActivePieces); i++)
    {
        /*
        std::cout <<"the moves at "<<i<<":\n"<< std::bitset<32>(moves_array.whiteMan[i]) << std::endl;
        std::cout <<"empty pieces: \n"<< std::bitset<32>(empty) << std::endl;
        std::cout << "the result of AND moves and empty: " << std::bitset<32>(moves_array.whiteMan[i] & empty) << std::endl;
*/
        if ((moves_array.whiteMan[i] & empty) != 0)
        {
            /*
            std::cout << "legal move first: " << std::countr_zero(moves_array.whiteMan[i])<<std::endl;;
            std::cout << "legal move second: " <<BITBOARD_SIZE- std::countl_zero(moves_array.whiteMan[i])<<std::endl;
*/
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.whiteMan[i])));
            legalMoves.push_back(Move(i,BITBOARD_SIZE- std::countl_zero(moves_array.whiteMan[i])));
 //           std::cout << "legal moves for " << i<<" : " << legalMoves.back() << std::endl;
            MakeMove(i,whitePiece,);
        }

        if ((moves_array.whiteManCaptures[i] & empty) != 0)
        {
            /*
            std::cout << "legal move first: " << std::countr_zero(moves_array.whiteMan[i])<<std::endl;;
            std::cout << "legal move second: " <<BITBOARD_SIZE- std::countl_zero(moves_array.whiteMan[i])<<std::endl;
*/
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.whiteManCaptures[i])));
            legalMoves.push_back(Move(i,BITBOARD_SIZE- std::countl_zero(moves_array.whiteManCaptures[i])));
 //           std::cout << "legal moves for " << i<<" : " << legalMoves.back() << std::endl;

            MakeMove(whitePiece, legalmove);
        }
    }
    
    
    for (auto& legalmove : legalMoves)
    {
    }

}



void findMoveBlack(Bitboard *blackPiece,Bitboard occupied,Bitboard empty){

    std::vector<Move> legalMoves;
    Bitboard ActivePieces = *blackPiece & blackActive;
/*
    std::cout << "moves at 11:\n" << std::bitset<32>(moves_array.blackMan[11]) << std::endl;
    std::cout << "moves at 8:\n" << std::bitset<32>(moves_array.blackMan[8]) << std::endl;
    std::cout << std::bitset<32>(ActivePieces) << std::endl;
    std::cout << std::countr_zero(ActivePieces) << std::endl;
    std::cout << "the index of first 1 starting form most significant bit\n";
    std::cout << std::countl_zero(ActivePieces) << std::endl;
    std::cout << "the index of first 1 starting form least significant bit\n";
*/
    for (int i = std::countr_zero(ActivePieces); i <= BITBOARD_SIZE-std::countl_zero(ActivePieces); i++)
    {
        /*
        std::cout <<"the moves at "<<i<<":\n"<< std::bitset<32>(moves_array.blackMan[i]) << std::endl;
        std::cout <<"empty pieces: \n"<< std::bitset<32>(empty) << std::endl;
        std::cout << "the result of AND moves and empty: " << std::bitset<32>(moves_array.blackMan[i] & empty) << std::endl;
*/
        if ((moves_array.blackMan[i] & empty) != 0)
        {
            /*
            std::cout << "legal move first: " << std::countr_zero(moves_array.blackMan[i])<<std::endl;;
            std::cout << "legal move second: " <<BITBOARD_SIZE- std::countl_zero(moves_array.blackMan[i])<<std::endl;
*/
            legalMoves.push_back(Move(i,std::countr_zero(moves_array.blackMan[i])));
            legalMoves.push_back(Move(i,BITBOARD_SIZE- std::countl_zero(moves_array.blackMan[i])));
 //           std::cout << "legal moves for " << i<<" : " << legalMoves.back() << std::endl;
        }
    }
    
    
    for (auto& legalmove : legalMoves)
    {
        std::cout << legalmove << std::endl;
        MakeMove(blackPiece, legalmove);
    }

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
    for (auto c : moves_array.whiteManCaptures) {
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

