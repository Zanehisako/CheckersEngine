#include <iostream>
#include <cstdint>
#include <array>

// We now use a 32‐bit bitboard to represent the 32 playable squares.
using Bitboard = uint32_t;

//--------------------------------------------------------------------
// Mapping for moves:
// For each square (0–31) we precompute the destination square (if any)
// for a move diagonally. (For red pieces, moves are upward; for black,
// moves are downward.) A value of -1 means no valid move in that direction.
//--------------------------------------------------------------------

// Initialize the move arrays. Our mapping is based on the idea that
// the 32 playable squares come from an 8x8 board where:
//   - Rows are numbered 0 (bottom) to 7 (top)
//   - Each row has 4 playable squares.
//   - On even rows (0, 2, 4, 6), playable squares are in columns 0,2,4,6.
//   - On odd rows (1, 3, 5, 7), playable squares are in columns 1,3,5,7.
// Move Generation Functions
struct MoveArrays {
    std::array<Bitboard, 32> whiteMan;
    std::array<Bitboard, 32> blackMan;
    std::array<Bitboard, 32> whiteKing;
    std::array<Bitboard, 32> blackKing;
};


consteval MoveArrays initMoveArrays() {
    MoveArrays moves{};  // Value-initialize all arrays
    for (int i = 0; i < 32; i++)
    {
        //setting up the white  man pieces by shifting them down left/right =>  and +4/5  
        if (i<28)
        {
            if (i==3 || 11 || 19||27)
            {
            moves.whiteMan[i] |= 1UL << (i + 4);
            }
            else if (i==4||12||20||28)
            {
            moves.whiteMan[i] |= 1UL << (i + 4);
            }
            else {
            moves.whiteMan[i] |= 1UL << (i + 4);
            moves.whiteMan[i] |= 1UL << (i + 5);
            }

        }
        //setting up the black man pieces by shifting them down left/right => -4/5 
        if (i>3)
        {
            if (i== 4 || 12 || 20 || 28 || 11 || 19 || 27)
            {
            moves.blackMan[i] |= 1UL << (i - 4);
            }
            else {
            moves.blackMan[i] |= 1UL << (i - 4);
            moves.blackMan[i] |= 1UL << (i - 5);
            }

        }
        }
    return moves;
}

constexpr auto moves_array = initMoveArrays();

//--------------------------------------------------------------------
// Utility: Print the 32-bit board as an 8x8 grid.
// Only the 32 playable squares are represented by a symbol ('1' if the
// corresponding bit is set, '.' if not). Non-playable squares are left blank.
//--------------------------------------------------------------------

void printBoard32(Bitboard board) {
    // Loop over rows 7 (top) to 0 (bottom)
    for (int r = 7; r >= 0; r--) {
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

//--------------------------------------------------------------------
// Main: Set up the initial position and generate moves.
//--------------------------------------------------------------------
int main() {
    clock_t before = clock();
    // Initial positions:
    // We place red pieces on the bottom three rows (rows 0, 1, 2 → indices 0..11)
    // and black pieces on the top three rows (rows 5,6,7 → indices 20..31).
    Bitboard redPieces = 0;
    for (int i = 0; i < 12; i++)
        redPieces |= (1 << i);
    
    Bitboard blackPieces = 0;
    for (int i = 20; i < 32; i++)
        blackPieces |= (1 << i);
    
    Bitboard occupied = redPieces | blackPieces;
    

    
    std::cout << "Initial Red Pieces (32-bit):\n";
    printBoard32(redPieces);
    
    std::cout << "Initial Black Pieces (32-bit):\n";
    printBoard32(blackPieces);
    
    std::cout << "Empty Playable Squares:\n";
    // The empty squares are the 32 bits not set in 'occupied'
    printBoard32(~occupied & 0xFFFFFFFF);
    
    std::cout << "All Black man moves:\n";
    for (int i = 0; i < moves_array.blackMan.size(); i++)
    {
        std::cout << i << std::endl;
    printBoard32(moves_array.blackMan[i]);
    }
    std::cout << "All White man moves:\n";
    for (int i = 0; i < moves_array.whiteMan.size(); i++)
    {
        std::cout << i << std::endl;
    printBoard32(moves_array.whiteMan[i]);
    }
    return 0;
}
