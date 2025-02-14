#include <iostream>
#include <cstdint>
#include <array>
#include <vector>
#include <bit>
#include <bitset>

constexpr auto BITBOARD_SIZE = 31;

// We now use a 32‐bit bitboard to represent the 32 playable squares.

using Bitboard = uint32_t;

struct Move {
    int from;
    int to;
    Move(int f,uint8_t t):from(f),to(t){}
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
    for (int i = 0; i < 32; i++)
    {
        //setting up the white  man pieces by shifting them down left/right =>  and +3/4   
        if (i<28)
        {
            if (i==3 ||i== 11 ||i== 19||i==27 || i==4||i==12||i==20||i==28)
            {
            moves.whiteMan[i] |= 1u << (i + 4);
            moves.whiteManCaptures[i] |= 1u << (i + 4);
            }
            else {
            moves.whiteMan[i] |= 1u << (i + 5);
            moves.whiteMan[i] |= 1u << (i + 4);
            moves.whiteManCaptures[i] |= 1u << (i + 5);
            moves.whiteManCaptures[i] |= 1u << (i + 4);
            }
            if (i+9<32)
            {
				if (i != 3 && i != 11 && i != 19 && i != 7 && i != 15 && i != 23 && i != 27)
                {
                    moves.whiteManCaptures[i] |= 1u << (i + 9);

                }
				if (i != 0 && i != 4 && i != 8 && i != 12 && i != 16 && i != 20 && i != 24)
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
            moves.blackManCaptures[i] |= 1u << (i - 4);
            }
            else {
            moves.blackMan[i] |= 1u << (i - 3);
            moves.blackMan[i] |= 1u << (i - 4);
            moves.blackManCaptures[i] |= 1u << (i - 3);
            moves.blackManCaptures[i] |= 1u << (i - 4);
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
Bitboard blackActive = 0x000F0000u;

//--------------------------------------------------------------------
// Utility: Print the 32-bit board as an 8x8 grid.
// Only the 32 playable squares are represented by a symbol ('1' if the
// corresponding bit is set, '.' if not). Non-playable squares are left blank.
//--------------------------------------------------------------------

void printBoard32(Bitboard board) {
    // Loop over rows 7 (top) to 0 (bottom)
    for (int r = 0; r <= 7; r++) {
        for (int c = 0; c < 8; c++) {
            bool playable = false;
            int index = -1;
            // Determine if (r,c) is a playable square.
            if (r % 2 == 0) {
                // Even rows: playable if column is even.
                if (c % 2 == 0) {
                    playable = true;
                    index = r * 4 + (c / 2);
                }
            } else {
                // Odd rows: playable if column is odd.
                if (c % 2 == 1) {
                    playable = true;
                    index = r * 4 + ((c - 1) / 2);
                }
            }
            if (playable) {
                char ch = ((board >> index) & 1) ? '1' : '.';
                std::cout << ch << " ";
            } else {
                std::cout << "  ";  // non-playable square
            }
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
}

inline static void MakeMove(Bitboard *board,Move move){
 
    std::cout << "the move is :" << move << std::endl;
    //this clear the bit
    *board &= ~(1UL << move.from);
    //this sets the bit
    *board |= 1UL << move.to;
    printBoard32(*board);

}

inline static void findCaptures(Bitboard *board,Move move){
 
    //XOR => ^=
    std::cout << "the move is :" << move << std::endl;
    //this clear the bit
    *board &= ~(1UL << move.from);
    //this sets the bit
    *board |= 1UL << move.to;
    printBoard32(*board);

}

void findMoveWhite(Bitboard *whitePiece,Bitboard occupied,Bitboard empty){

    std::vector<Move> legalMoves;
    Bitboard ActivePieces = *whitePiece & whiteActive;
/*
    std::cout << "Active pieces:\n";
    printBoard32(ActivePieces);
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
        }
    }
    
    
    for (auto& legalmove : legalMoves)
    {
        std::cout << legalmove << std::endl;
        MakeMove(whitePiece, legalmove);
    }

}

void findMoveBlack(Bitboard *blackPiece,Bitboard occupied,Bitboard empty){

    std::vector<Move> legalMoves;
    Bitboard ActivePieces = *blackPiece & blackActive;
/*
    std::cout << "Active pieces:\n";
    printBoard32(ActivePieces);
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
    printBoard32(whitePieces);
    
    std::cout << "Initial Black Pieces (32-bit):\n";
    printBoard32(blackPieces);

    std::cout << "Initial White Pieces captures (32-bit):\n";
    for (auto c : moves_array.whiteManCaptures) {
        std::cout << std::bitset<32>(c) << std::endl;
    }
    
    std::cout << "Empty Playable Squares:\n";
    // The empty squares are the 32 bits not set in 'occupied'
    printBoard32(empty);
    for (size_t i = 0; i < 10000000; i++)
    {
    findMoveWhite(&whitePieces, occupied, empty);
    findMoveBlack(&whitePieces, occupied, empty);
    }
    std::cout << "White Pieces (32-bit):\n";
    printBoard32(whitePieces);
    std::cout << "it took" << (float)(clock() - before) / CLOCKS_PER_SEC <<"s"<< std::endl;

    return 0;
}
