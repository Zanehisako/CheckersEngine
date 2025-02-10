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
    std::array<int, 32> redMoveLeft;
    std::array<int, 32> redMoveRight;
    std::array<int, 32> blackMoveLeft;
    std::array<int, 32> blackMoveRight;
};


constexpr MoveArrays initMoveArrays() {
    MoveArrays moves{};  // Value-initialize all arrays

    for (int i = 0; i < 32; i++) {
        int row = i / 4;        // which board row (0 to 7)
        int pos = i % 4;        // position within that row (0 to 3)
        int col = (row % 2 == 0) ? pos * 2 : pos * 2 + 1;  // even row: 0,2,4,6; odd row: 1,3,5,7

        // Helper lambda to calculate destination index
        constexpr auto calcDestIndex = [](int newRow, int newCol) -> int {
            if (newRow % 2 == 0) {
                // Even row: check for even columns
                if (newCol >= 0 && newCol <= 6 && (newCol % 2 == 0)) {
                    return newRow * 4 + (newCol / 2);
                }
            } else {
                // Odd row: check for odd columns
                if (newCol >= 1 && newCol <= 7 && (newCol % 2 == 1)) {
                    return newRow * 4 + ((newCol - 1) / 2);
                }
            }
            return -1;
        };

        // Red moves (moving upward)
        int newRow = row + 1;
        moves.redMoveLeft[i] = (newRow < 8) ? calcDestIndex(newRow, col - 1) : -1;
        moves.redMoveRight[i] = (newRow < 8) ? calcDestIndex(newRow, col + 1) : -1;

        // Black moves (moving downward)
        newRow = row - 1;
        moves.blackMoveLeft[i] = (newRow >= 0) ? calcDestIndex(newRow, col - 1) : -1;
        moves.blackMoveRight[i] = (newRow >= 0) ? calcDestIndex(newRow, col + 1) : -1;
    }

    return moves;
}

constexpr auto moves_array = initMoveArrays();
//--------------------------------------------------------------------
// Given a bitboard of red pieces and the overall occupied squares,
// generate the destination squares (as bits) for non‐capturing moves.
Bitboard generateRedMoves(Bitboard redPieces, Bitboard occupied) {
    Bitboard moves = 0;
    for (int i = 0; i < 32; i++) {
        if (redPieces & (1 << i)) {
            int dest = moves_array.redMoveLeft[i];
            if (dest != -1 && ((occupied >> dest) & 1) == 0)
                moves |= (1 << dest);
            dest = moves_array.redMoveRight[i];
            if (dest != -1 && ((occupied >> dest) & 1) == 0)
                moves |= (1 << dest);
        }
    }
    return moves;
}

// Similarly, generate moves for black pieces (moving downward).
Bitboard generateBlackMoves(Bitboard blackPieces, Bitboard occupied) {
    Bitboard moves = 0;
    for (int i = 0; i < 32; i++) {
        if (blackPieces & (1 << i)) {
            int dest = moves_array.blackMoveLeft[i];
            if (dest != -1 && ((occupied >> dest) & 1) == 0)
                moves |= (1 << dest);
            dest = moves_array.blackMoveRight[i];
            if (dest != -1 && ((occupied >> dest) & 1) == 0)
                moves |= (1 << dest);
        }
    }
    return moves;
}

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
    
    Bitboard redMoves = generateRedMoves(redPieces, occupied);
    Bitboard blackMoves = generateBlackMoves(blackPieces, occupied);
    
    std::cout << "Initial Red Pieces (32-bit):\n";
    printBoard32(redPieces);
    
    std::cout << "Initial Black Pieces (32-bit):\n";
    printBoard32(blackPieces);
    
    std::cout << "Empty Playable Squares:\n";
    // The empty squares are the 32 bits not set in 'occupied'
    printBoard32(~occupied & 0xFFFFFFFF);
    
    std::cout << "Generated Red Moves (destinations):\n";
    printBoard32(redMoves);
    
    std::cout << "Generated Black Moves (destinations):\n";
    printBoard32(blackMoves);
    
    return 0;
}
