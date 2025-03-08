﻿#include <algorithm>
#include <array>
#include <bit>
#include <thread>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
#include <sio_client.h>

#ifdef _MSC_VER
#include <intrin.h>
#define __builtin_popcount _mm_popcnt_u32
#endif

// Using 32-bit bitboards - more efficient for this application
using Bitboard = uint32_t;
// Add these constants near your other definitions
constexpr Bitboard CENTER_SQUARES = (1U << 10) | (1U << 11) | (1U << 14) | (1U << 15) |
                                   (1U << 16) | (1U << 17) | (1U << 20) | (1U << 21);
constexpr Bitboard EDGE_SQUARES = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3) |
                                 (1U << 4) | (1U << 7) | (1U << 24) | (1U << 27) |
                                 (1U << 28) | (1U << 29) | (1U << 30) | (1U << 31);
constexpr Bitboard PROMOTION_ZONE_WHITE = (1U << 28) | (1U << 29) | (1U << 30) | (1U << 31);
constexpr Bitboard PROMOTION_ZONE_BLACK = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3);
// Define a struct to hold x and y values
struct Point {
    uint8_t x;
    uint8_t y;
};

std::unordered_map<uint8_t, std::string> boardMapping = {
    {0, "10"}, {1, "30"}, {2, "50"}, {3, "70"},
    {4, "01"}, {5, "21"}, {6, "41"}, {7, "61"},
    {8, "12"}, {9, "32"}, {10, "52"}, {11, "72"},
    {12, "03"}, {13, "23"}, {14, "43"}, {15, "63"},
    {16, "14"}, {17, "34"}, {18, "54"}, {19, "74"},
    {20, "05"}, {21, "25"}, {22, "45"}, {23, "65"},
    {24, "16"}, {25, "36"}, {26, "56"}, {27, "76"},
    {28, "07"}, {29, "27"}, {30, "47"}, {31, "67"}
};

// Mapping from 32-bit index (0-31) to (x, y) in a 64-bit board
std::unordered_map<uint8_t, Point> positions_indexes = {
	{0,  {1, 0}}, {1,  {3, 0}}, {2,  {5, 0}}, {3,  {7, 0}},
	{4,  {0, 1}}, {5,  {2, 1}}, {6,  {4, 1}}, {7,  {6, 1}},
	{8,  {1, 2}}, {9,  {3, 2}}, {10, {5, 2}}, {11, {7, 2}},
	{12, {0, 3}}, {13, {2, 3}}, {14, {4, 3}}, {15, {6, 3}},
	{16, {1, 4}}, {17, {3, 4}}, {18, {5, 4}}, {19, {7, 4}},
	{20, {0, 5}}, {21, {2, 5}}, {22, {4, 5}}, {23, {6, 5}},
	{24, {1, 6}}, {25, {3, 6}}, {26, {5, 6}}, {27, {7, 6}},
	{28, {0, 7}}, {29, {2, 7}}, {30, {4, 7}}, {31, {6, 7}}
};

enum MoveType : uint8_t {
    URCapture, ULCapture, DRCapture, DLCapture,
    URMove, ULMove, DRMove, DLMove
};

struct Move {
    uint8_t from, to;
    MoveType type;
    Move() = default;
    Move(uint8_t f, uint8_t t, MoveType mt) : from(f), to(t), type(mt) {}
};
std::ostream& operator<<(std::ostream &os, const Move &m) {
    os << "from:" << static_cast<int>(m.from)
       << " to:" << static_cast<int>(m.to);
    return os;
}

struct GameState {
    Bitboard white, black, kings, empty;
    bool whiteToMove;
    uint32_t hash;
    GameState() : white(0), black(0), kings(0), empty(0), whiteToMove(true), hash(0) {}
    GameState(Bitboard w, Bitboard b, Bitboard k, Bitboard e, bool wtm, uint32_t h)
        : white(w), black(b), kings(k), empty(e), whiteToMove(wtm), hash(h) {}

    inline Bitboard occupied() const noexcept { return white | black; }
    inline void updateEmpty() noexcept { empty = ~occupied(); }
};

// Compile-time random number generator
constexpr uint32_t lcg(uint32_t seed) {
    return seed * 1664525UL + 1013904223UL;
}

constexpr std::array<uint32_t, 32> generateZobristKeys(uint32_t seed) {
    std::array<uint32_t, 32> keys{};
    for (auto& key : keys) {
        seed = lcg(seed);
        key = seed;
    }
    return keys;
}

constexpr bool repatingMove(std::vector<uint32_t> history) {
    if (history.back() == history[history.size() -2])
    {
        return true;
    }
}

constexpr int indexFromRC(int row, int col) {
    if (row < 0 || row >= 8 || col < 0 || col >= 8)
        return -1;
    if ((row + col) % 2 == 0)  // Only dark squares are playable
        return -1;
    return (row * 4) + (col / 2);
}

constexpr void rcFromIndex(int i, int &row, int &col) {
    row = i / 4;
    int j = i % 4;
    col = 2 * j + ((row & 1) ? 0 : 1);
}

// Precompute square coordinates (row,col) for each playable square
constexpr std::array<std::pair<int, int>, 32> squareCoords = []{
    std::array<std::pair<int, int>, 32> coords{};
    for (int i = 0; i < 32; i++) {
        int r, c;
        rcFromIndex(i, r, c);
        coords[i] = {r, c};
    }
    return coords;
}();

constexpr std::array<uint32_t, 32> zobrist_white_man = generateZobristKeys(12345);
constexpr std::array<uint32_t, 32> zobrist_white_king = generateZobristKeys(67890);
constexpr std::array<uint32_t, 32> zobrist_black_man = generateZobristKeys(54321);
constexpr std::array<uint32_t, 32> zobrist_black_king = generateZobristKeys(98765);
constexpr uint32_t zobrist_side_to_move = 0x12345678;

uint32_t computeInitialHash(const GameState& state) {
    uint32_t hash = 0;
    for (int pos = 0; pos < 32; pos++) {
        Bitboard bit = 1U << pos;
        if (state.white & bit) {
            if (state.kings & bit) hash ^= zobrist_white_king[pos];
            else hash ^= zobrist_white_man[pos];
        } else if (state.black & bit) {
            if (state.kings & bit) hash ^= zobrist_black_king[pos];
            else hash ^= zobrist_black_man[pos];
        }
    }
    if (state.whiteToMove) hash ^= zobrist_side_to_move;
    return hash;
}


// Pre-computed move masks for each square and direction
struct MoveArrays {
    std::array<Bitboard, 32> whiteManLeft, whiteManRight;
    std::array<Bitboard, 32> blackManLeft, blackManRight;
    std::array<Bitboard, 32> whiteManCaptureLeft, whiteManCaptureRight;
    std::array<Bitboard, 32> blackManCaptureLeft, blackManCaptureRight;
    std::array<Bitboard, 32> kingCaptureUL, kingCaptureUR, kingCaptureDL, kingCaptureDR;
    std::array<Bitboard, 32> capturePieces;
};

consteval MoveArrays initMoveArrays() {
    MoveArrays moves{};
    for (int i = 0; i < 32; i++) {
        int row, col;
        rcFromIndex(i, row, col);
        
        // White men move down
        if (row < 7) {
            if (col > 0) {
                int target = indexFromRC(row + 1, col - 1);
                if (target >= 0) moves.whiteManLeft[i] = 1U << target;
            }
            if (col < 7) {
                int target = indexFromRC(row + 1, col + 1);
                if (target >= 0) moves.whiteManRight[i] = 1U << target;
            }
        }
        
        // Black men move up
        if (row > 0) {
            if (col > 0) {
                int target = indexFromRC(row - 1, col - 1);
                if (target >= 0) moves.blackManLeft[i] = 1U << target;
            }
            if (col < 7) {
                int target = indexFromRC(row - 1, col + 1);
                if (target >= 0) moves.blackManRight[i] = 1U << target;
            }
        }
        
        // White men captures
        if (row < 6) {
            if (col > 1) {
                int target = indexFromRC(row + 2, col - 2);
                int middle = indexFromRC(row + 1, col - 1);
                if (target >= 0 && middle >= 0) {
                    moves.whiteManCaptureLeft[i] = 1U << target;
                    moves.capturePieces[i] |= 1U << middle;
                }
            }
            if (col < 6) {
                int target = indexFromRC(row + 2, col + 2);
                int middle = indexFromRC(row + 1, col + 1);
                if (target >= 0 && middle >= 0) {
                    moves.whiteManCaptureRight[i] = 1U << target;
                    moves.capturePieces[i] |= 1U << middle;
                }
            }
        }
        
        // Black men captures
        if (row > 1) {
            if (col > 1) {
                int target = indexFromRC(row - 2, col - 2);
                int middle = indexFromRC(row - 1, col - 1);
                if (target >= 0 && middle >= 0) {
                    moves.blackManCaptureLeft[i] = 1U << target;
                    moves.capturePieces[i] |= 1U << middle;
                }
            }
            if (col < 6) {
                int target = indexFromRC(row - 2, col + 2);
                int middle = indexFromRC(row - 1, col + 1);
                if (target >= 0 && middle >= 0) {
                    moves.blackManCaptureRight[i] = 1U << target;
                    moves.capturePieces[i] |= 1U << middle;
                }
            }
        }
        
        // King captures in all directions reuse the man capture moves
        moves.kingCaptureUL[i] = moves.blackManCaptureLeft[i];
        moves.kingCaptureUR[i] = moves.blackManCaptureRight[i];
        moves.kingCaptureDL[i] = moves.whiteManCaptureLeft[i];
        moves.kingCaptureDR[i] = moves.whiteManCaptureRight[i];
    }
    return moves;
}

constexpr auto moves_array = initMoveArrays();

struct MoveList {
    std::array<Move, 44> moves;
    int count = 0;
    inline void add(uint8_t from, uint8_t to, MoveType type) noexcept {
        moves[count++] = Move(from, to, type);
    }
    inline const void print() {
        for (int i = 0; i < count; i++)
        {
            std::cout << "move:" << moves[i] << std::endl;
        }
    }
    inline const Move* begin() const noexcept { return moves.data(); }
    inline const Move* end() const noexcept { return moves.data() + count; }
};

constexpr int INF = 1'000'000;

// Helper: Check if a capture move is legal in one direction
// (avoids repeating similar code in capture-check and move-generation)
inline bool checkCaptureDirection(uint8_t square, Bitboard mask,
                                  const GameState& state, Bitboard opponents) noexcept {
    if (!mask) return false;
    int toSquare = std::countr_zero(mask);
    auto [fromRow, fromCol] = squareCoords[square];
    auto [toRow, toCol] = squareCoords[toSquare];
    int midRow = (fromRow + toRow) >> 1;
    int midCol = (fromCol + toCol) >> 1;
    int midPos = indexFromRC(midRow, midCol);
    return (midPos >= 0 && (opponents & (1U << midPos)) && (state.empty & mask));
}

// Check if a piece has any legal capture moves available
inline bool hasCaptureMove(const GameState& state, uint8_t square) noexcept {
    Bitboard piece = 1U << square;
    bool isKing = (state.kings & piece) != 0;
    Bitboard opponents = state.whiteToMove ? state.black : state.white;
    
    if (state.whiteToMove) {
        if (isKing) {
            return checkCaptureDirection(square, moves_array.kingCaptureUL[square], state, opponents) ||
                   checkCaptureDirection(square, moves_array.kingCaptureUR[square], state, opponents) ||
                   checkCaptureDirection(square, moves_array.kingCaptureDL[square], state, opponents) ||
                   checkCaptureDirection(square, moves_array.kingCaptureDR[square], state, opponents);
        } else {
            return checkCaptureDirection(square, moves_array.whiteManCaptureLeft[square], state, opponents) ||
                   checkCaptureDirection(square, moves_array.whiteManCaptureRight[square], state, opponents);
        }
    } else {
        if (isKing) {
            return checkCaptureDirection(square, moves_array.kingCaptureUL[square], state, opponents) ||
                   checkCaptureDirection(square, moves_array.kingCaptureUR[square], state, opponents) ||
                   checkCaptureDirection(square, moves_array.kingCaptureDL[square], state, opponents) ||
                   checkCaptureDirection(square, moves_array.kingCaptureDR[square], state, opponents);
        } else {
            return checkCaptureDirection(square, moves_array.blackManCaptureLeft[square], state, opponents) ||
                   checkCaptureDirection(square, moves_array.blackManCaptureRight[square], state, opponents);
        }
    }
}

// Check if any capture move exists for the current side
inline bool isCapturePossible(const GameState& state) noexcept {
    Bitboard pieces = state.whiteToMove ? state.white : state.black;
    while (pieces) {
        uint8_t square = std::countr_zero(pieces);
        if (hasCaptureMove(state, square)) return true;
        pieces &= pieces - 1; // Clear LSB
    }
    return false;
}

// Generate regular (non-capturing) moves for a piece
inline void generateRegularMoves(const GameState& state, uint8_t square, MoveList& moveList) noexcept {
    Bitboard piece = 1U << square;
    bool isKing = (state.kings & piece) != 0;
    
    if (state.whiteToMove) {
        Bitboard targets = isKing ?
            ((moves_array.whiteManLeft[square] | moves_array.whiteManRight[square] |
              moves_array.blackManLeft[square] | moves_array.blackManRight[square]) & state.empty) :
            ((moves_array.whiteManLeft[square] | moves_array.whiteManRight[square]) & state.empty);
        
        if (moves_array.whiteManLeft[square] & targets)
            moveList.add(square, std::countr_zero(moves_array.whiteManLeft[square]), DLMove);
        if (moves_array.whiteManRight[square] & targets)
            moveList.add(square, std::countr_zero(moves_array.whiteManRight[square]), DRMove);
        if (isKing) {
            if (moves_array.blackManLeft[square] & targets)
                moveList.add(square, std::countr_zero(moves_array.blackManLeft[square]), ULMove);
            if (moves_array.blackManRight[square] & targets)
                moveList.add(square, std::countr_zero(moves_array.blackManRight[square]), URMove);
        }
    } else {
        Bitboard targets = isKing ?
            ((moves_array.whiteManLeft[square] | moves_array.whiteManRight[square] |
              moves_array.blackManLeft[square] | moves_array.blackManRight[square]) & state.empty) :
            ((moves_array.blackManLeft[square] | moves_array.blackManRight[square]) & state.empty);
        
        if (moves_array.blackManLeft[square] & targets)
            moveList.add(square, std::countr_zero(moves_array.blackManLeft[square]), ULMove);
        if (moves_array.blackManRight[square] & targets)
            moveList.add(square, std::countr_zero(moves_array.blackManRight[square]), URMove);
        if (isKing) {
            if (moves_array.whiteManLeft[square] & targets)
                moveList.add(square, std::countr_zero(moves_array.whiteManLeft[square]), DLMove);
            if (moves_array.whiteManRight[square] & targets)
                moveList.add(square, std::countr_zero(moves_array.whiteManRight[square]), DRMove);
        }
    }
}

// Generate capture moves for a piece using our helper
inline void generateCaptureMoves(const GameState& state, uint8_t square, MoveList& moveList) noexcept {
    Bitboard piece = 1U << square;
    bool isKing = (state.kings & piece) != 0;
    Bitboard opponents = state.whiteToMove ? state.black : state.white;
    
    if (state.whiteToMove) {
        if (isKing) {
            if (checkCaptureDirection(square, moves_array.kingCaptureUL[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.kingCaptureUL[square]), ULCapture);
            if (checkCaptureDirection(square, moves_array.kingCaptureUR[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.kingCaptureUR[square]), URCapture);
            if (checkCaptureDirection(square, moves_array.kingCaptureDL[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.kingCaptureDL[square]), DLCapture);
            if (checkCaptureDirection(square, moves_array.kingCaptureDR[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.kingCaptureDR[square]), DRCapture);
        } else {
            if (checkCaptureDirection(square, moves_array.whiteManCaptureLeft[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.whiteManCaptureLeft[square]), DLCapture);
            if (checkCaptureDirection(square, moves_array.whiteManCaptureRight[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.whiteManCaptureRight[square]), DRCapture);
        }
    } else {
        if (isKing) {
            if (checkCaptureDirection(square, moves_array.kingCaptureUL[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.kingCaptureUL[square]), ULCapture);
            if (checkCaptureDirection(square, moves_array.kingCaptureUR[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.kingCaptureUR[square]), URCapture);
            if (checkCaptureDirection(square, moves_array.kingCaptureDL[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.kingCaptureDL[square]), DLCapture);
            if (checkCaptureDirection(square, moves_array.kingCaptureDR[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.kingCaptureDR[square]), DRCapture);
        } else {
            if (checkCaptureDirection(square, moves_array.blackManCaptureLeft[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.blackManCaptureLeft[square]), ULCapture);
            if (checkCaptureDirection(square, moves_array.blackManCaptureRight[square], state, opponents))
                moveList.add(square, std::countr_zero(moves_array.blackManCaptureRight[square]), URCapture);
        }
    }
}

// Generate all moves (capture moves if available; otherwise, regular moves)
inline MoveList generateMoves(const GameState& state) noexcept {
    MoveList moveList;
    Bitboard pieces = state.whiteToMove ? state.white : state.black;
    bool mustCapture = isCapturePossible(state);
    
    while (pieces) {
        uint8_t square = std::countr_zero(pieces);
        if (mustCapture)
            generateCaptureMoves(state, square, moveList);
        else
            generateRegularMoves(state, square, moveList);
        pieces &= pieces - 1;
    }
    return moveList;
}

// Apply a move to create a new state
GameState applyMove(const GameState& state, const Move& move) noexcept {
    GameState newState = state;
    Bitboard fromBit = 1U << move.from;
    Bitboard toBit = 1U << move.to;
     // Update hash for moving piece
    if (state.whiteToMove) {
        newState.hash ^= (state.kings & fromBit) ? zobrist_white_king[move.from] : zobrist_white_man[move.from];
        newState.white = (newState.white & ~fromBit) | toBit;
        newState.hash ^= (state.kings & fromBit) ? zobrist_white_king[move.to] : zobrist_white_man[move.to];
    } else {
        newState.hash ^= (state.kings & fromBit) ? zobrist_black_king[move.from] : zobrist_black_man[move.from];
        newState.black = (newState.black & ~fromBit) | toBit;
        newState.hash ^= (state.kings & fromBit) ? zobrist_black_king[move.to] : zobrist_black_man[move.to];
    }

    // Handle captures
    if (move.type <= DLCapture) {
        auto [fromRow, fromCol] = squareCoords[move.from];
        auto [toRow, toCol] = squareCoords[move.to];
        int midRow = (fromRow + toRow) >> 1;
        int midCol = (fromCol + toCol) >> 1;
        int midPos = indexFromRC(midRow, midCol);
        if (midPos >= 0) {
            Bitboard midBit = 1U << midPos;
            if (state.whiteToMove)
                newState.black &= ~midBit;
            else
                newState.white &= ~midBit;
            newState.kings &= ~midBit;
        }
    }
    
    // Promotion check
    if (!(newState.kings & toBit)) {
        if ((state.whiteToMove && (move.to >= 28)) ||
            (!state.whiteToMove && (move.to <= 3))) {
            newState.kings |= toBit;
        }
    }
    
    newState.updateEmpty();
    //change this if u want to have game between the bot its self
    newState.whiteToMove = !newState.whiteToMove;
    newState.hash = state.hash ^ zobrist_side_to_move;
    return newState;
}


// Simple evaluation: piece count weighted by value
// Enhanced evaluation function
inline int evaluateState(const GameState& state) noexcept {
    // Early game-over checks
    if (state.white == 0) return state.whiteToMove ? -INF : INF;
    if (state.black == 0) return state.whiteToMove ? INF : -INF;

    // Material count
    int whiteMen = __builtin_popcount(state.white & ~state.kings);
    int blackMen = __builtin_popcount(state.black & ~state.kings);
    int whiteKings = __builtin_popcount(state.white & state.kings);
    int blackKings = __builtin_popcount(state.black & state.kings);
    
    int materialScore = (whiteMen * 100 + whiteKings * 250) -
                       (blackMen * 100 + blackKings * 250);

    // Positional factors
    int whitePos = 0, blackPos = 0;
    
    // Center control bonus (encourages controlling the middle)
    whitePos += __builtin_popcount(state.white & CENTER_SQUARES) * 10;
    blackPos += __builtin_popcount(state.black & CENTER_SQUARES) * 10;
    
    // Edge penalty (discourages pieces on edges)
    whitePos -= __builtin_popcount(state.white & EDGE_SQUARES) * 5;
    blackPos -= __builtin_popcount(state.black & EDGE_SQUARES) * 5;
    
    // Advancement bonus (encourages moving toward promotion)
    whitePos += __builtin_popcount(state.white & PROMOTION_ZONE_WHITE) * 20;
    blackPos += __builtin_popcount(state.black & PROMOTION_ZONE_BLACK) * 20;
    
    // Mobility bonus (counts legal non-capture moves)
    MoveList moves = generateMoves(state);
    int mobility = moves.count * 5;  // Small bonus per legal move
    
    // King protection (bonus for men near promotion that are safe)
    int whiteSafeMen = __builtin_popcount(state.white & ~state.kings & PROMOTION_ZONE_WHITE & ~state.black);
    int blackSafeMen = __builtin_popcount(state.black & ~state.kings & PROMOTION_ZONE_BLACK & ~state.white);
    
    // Combine scores
    int whiteScore = materialScore + whitePos + (state.whiteToMove ? mobility : 0) + whiteSafeMen * 15;
    int blackScore = -materialScore + blackPos + (!state.whiteToMove ? mobility : 0) + blackSafeMen * 15;
    
    // Piece-square table for fine-tuned positioning
    static constexpr std::array<int, 32> pieceSquareTable = {
        4, 4, 4, 4,    // Row 0
        2, 3, 3, 2,    // Row 1
        2, 3, 3, 2,    // Row 2
        1, 2, 2, 1,    // Row 3
        0, 1, 1, 0,    // Row 4
        -1, 0, 0, -1,  // Row 5
        -2, -1, -1, -2,// Row 6
        -4, -4, -4, -4 // Row 7
    };
    
    int pstScore = 0;
    Bitboard whitePieces = state.white;
    Bitboard blackPieces = state.black;
    while (whitePieces) {
        uint8_t pos = std::countr_zero(whitePieces);
        pstScore += pieceSquareTable[pos];
        whitePieces &= whitePieces - 1;
    }
    while (blackPieces) {
        uint8_t pos = std::countr_zero(blackPieces);
        pstScore -= pieceSquareTable[31 - pos];  // Mirror for black
        blackPieces &= blackPieces - 1;
    }
    
    // Final evaluation
    int totalScore = (whiteScore - blackScore) + pstScore * 2;
    return state.whiteToMove ? totalScore : -totalScore;
}

// Transposition table for alpha-beta search
struct TranspositionTable {
    enum Flag { EXACT, LOWER, UPPER };
    struct Entry {
        uint32_t hash;
        int16_t eval;
        uint8_t depth;
        Flag flag;
    };
    
    std::vector<Entry> table;
    size_t sizeMask;
    TranspositionTable(size_t size) : table(size), sizeMask(size - 1) {
        if ((size & sizeMask) != 0)
            throw std::invalid_argument("Size must be a power of two");
    }
    
    inline bool lookup(uint32_t hash, int depth, int& eval, Flag& flag) noexcept {
        Entry& entry = table[hash & sizeMask];
        if (entry.hash == hash && entry.depth >= depth) {
            eval = entry.eval;
            flag = entry.flag;
            return true;
        }
        return false;
    }
    
    inline void store(uint32_t hash, int depth, int eval, Flag flag) noexcept {
        Entry& entry = table[hash & sizeMask];
        if (depth >= entry.depth) {
            entry.hash = hash;
            entry.depth = depth;
            entry.eval = eval;
            entry.flag = flag;
        }
    }
};

void printGameState(const GameState& state) {
    std::cout << "  0 1 2 3 4 5 6 7\n";
    for (int row = 0; row < 8; row++) {
        std::cout << row << " ";
        for (int col = 0; col < 8; col++) {
            if ((row + col) % 2 == 0)
                std::cout << "  ";
            else {
                int idx = indexFromRC(row, col);
                char cell = '.';
                if (state.white & (1U << idx))
                    cell = (state.kings & (1U << idx)) ? 'W' : 'w';
                else if (state.black & (1U << idx))
                    cell = (state.kings & (1U << idx)) ? 'B' : 'b';
                std::cout << cell << " ";
            }
        }
        std::cout << "\n";
    }
    std::cout << (state.whiteToMove ? "White" : "Black") << " to move\n";
}

// Alpha-beta minimax search with transposition table
inline int minimax(const GameState& state, int depth, int alpha, int beta,
                   TranspositionTable& tt) noexcept {
    if (depth == 0)
        return evaluateState(state);
    
    int ttEval;
    TranspositionTable::Flag ttFlag;
    if (tt.lookup(state.hash, depth, ttEval, ttFlag)) {
        if (ttFlag == TranspositionTable::EXACT)
            return ttEval;
        if (ttFlag == TranspositionTable::LOWER && ttEval >= beta)
            return ttEval;
        if (ttFlag == TranspositionTable::UPPER && ttEval <= alpha)
            return ttEval;
    }
    
    MoveList moves = generateMoves(state);
    if (moves.count == 0)
        return state.whiteToMove ? -INF : INF;
    
    int bestEval = state.whiteToMove ? -INF : INF;
    for (const Move* m = moves.begin(); m != moves.end(); ++m) {
        GameState child = applyMove(state, *m);
        int eval = minimax(child, depth - 1, alpha, beta, tt);
        if (state.whiteToMove) {
            bestEval = std::max(bestEval, eval);
            alpha = std::max(alpha, eval);
        } else {
            bestEval = std::min(bestEval, eval);
            beta = std::min(beta, eval);
        }
        if (beta <= alpha)
            break;
    }
    
    TranspositionTable::Flag flag;
    if (bestEval <= alpha)
        flag = TranspositionTable::UPPER;
    else if (bestEval >= beta)
        flag = TranspositionTable::LOWER;
    else
        flag = TranspositionTable::EXACT;
    
    tt.store(state.hash, depth, bestEval, flag);
    return bestEval;
}

Move findBestMove(const GameState& state, int depth,std::vector<uint32_t> gameHistory) {
    MoveList moves = generateMoves(state);
    std::cout << "moves length:" << moves.count << std::endl;
    if (moves.count == 0)
        throw std::runtime_error("No legal moves available");
    
    TranspositionTable tt(1 << 20);  // 1M entries
    Move bestMove = moves.moves[0];
    int bestValue = state.whiteToMove ? -INF : INF;
    int alpha = -INF, beta = INF;
    
    for (const Move* m = moves.begin(); m != moves.end(); ++m) {
        GameState child = applyMove(state, *m);
        int moveValue = minimax(child, depth - 1, alpha, beta, tt);
         // Check if this state has been repeated more than twice
        std::cout << "child hash: "<<child.hash<<std::endl;
        gameHistory.push_back(child.hash);
		bool found = std::find(gameHistory.begin(), gameHistory.end(), child.hash) != gameHistory.end();
		std::cout << "Found: " << (found ? "Yes" : "No") << std::endl;
        int repeatCount = std::count(gameHistory.begin(), gameHistory.end(), child.hash);
        std::cout << "repeated count :"<<repeatCount<<std::endl;
        if (repeatCount >= 1) {
            std::cout << "repeated move\n";
            if (state.whiteToMove) {
                moveValue -= 1000000;  // Punish for repeating (white is maximizing)
            } else {
                moveValue += 1000000;  // Punish for repeating (black is minimizing)
            }
        }
        if (state.whiteToMove) {
            if (moveValue > bestValue) {
                bestValue = moveValue;
                bestMove = *m;
            }
            alpha = std::max(alpha, bestValue);
        } else {
            if (moveValue < bestValue) {
                bestValue = moveValue;
                bestMove = *m;
            }
            beta = std::min(beta, bestValue);
        }
    }
    
    std::cout << "Best move evaluation: " << bestValue << "\n";
    return bestMove;
}



inline bool isKing(uint8_t y,bool white) {
    switch (white)
    {
    case true:
    if (y==7)
    {
        return true;
    }
    return false;

    case false:
    if (y==0)
    {
        return true;
    }
    return false;
    }

}


//////////////////////////////////////////////////////////////////////////////
// Socket.IO Client & Integration Code
//////////////////////////////////////////////////////////////////////////////

// The server sends board updates as a two-element array of Position arrays.
// Previously, board[0] held white pieces (top rows) and board[1] held black pieces (bottom rows).
// Now that we want white on top and black at the bottom, we assign directly:
void updateGameStateFromJSON(const sio::message::ptr &msg, GameState &state) {
    // Reset state.
    state.white = 0;
    state.black = 0;
    state.kings = 0;

    if (msg->get_flag() != sio::message::flag_array) {
        std::cerr << "Board update message is not an array!" << std::endl;
        return;
    }
    std::vector<sio::message::ptr> boardArr = msg->get_vector();
    if (boardArr.size() < 2) {
        std::cerr << "Board update does not contain two arrays." << std::endl;
        return;
    }

    // Board[0] holds white pieces, Board[1] holds black pieces.
    auto whiteArr = boardArr.at(1)->get_vector();
    for (auto &pieceMsg : whiteArr) {
        if (pieceMsg->get_flag() != sio::message::flag_object)
            continue;
        auto m = pieceMsg->get_map();
        int x = m["x"]->get_int();
        int y = m["y"]->get_int();
        bool king = m["king"]->get_bool();
        int idx = indexFromRC(y, x);
        if (idx >= 0) {
            state.white |= (1U << idx);
            if (king)
                state.kings |= (1U << idx);
        }
    }
    auto blackArr = boardArr.at(0)->get_vector();
    for (auto &pieceMsg : blackArr) {
        if (pieceMsg->get_flag() != sio::message::flag_object)
            continue;
        auto m = pieceMsg->get_map();
        int x = m["x"]->get_int();
        int y = m["y"]->get_int();
        bool king = m["king"]->get_bool();
        int idx = indexFromRC(y, x);
        if (idx >= 0) {
            state.black |= (1U << idx);
            if (king)
                state.kings |= (1U << idx);
        }
    }
    state.updateEmpty();
    state.hash = computeInitialHash(state);
    std::cout << "GameState updated from board JSON." << std::endl;
    printGameState(state);
}


//////////////////////////////////////////////////////////////////////////////
// CheckersClient class that integrates the engine with the Socket.IO server.
//////////////////////////////////////////////////////////////////////////////

class CheckersClient {
public:
    sio::client client;
    GameState gameState;
    bool isWhite;
    int searchDepth=1;
	std::vector<uint32_t> moveHistoryWhite;  // Track white state hashes 
	std::vector<uint32_t> moveHistoryBlack;  // Track black state hashes

    CheckersClient() { }

    // Connect to the Socket.IO server.
    void connectToServer(const std::string &url) {
        client.set_open_listener([this]() {
            std::cout << "Connected to server" << std::endl;
            int choice;
            std::cout << "1: create room\n2: join room\n";
            std::cin >> choice;
            std::string room;
           std::cout << "Enter room name:\n";
           std::cin >> room;
           std::cout << "Enter Search depth:\n";
           std::cin >> searchDepth;
            switch (choice)
            {
            case 1:
                std::cout << "Creating a room\n";
                client.socket()->emit("create room", room);
                isWhite= true;
                break;
            case 2:
                std::cout << "Joining as a player\n";
                client.socket()->emit("join room as player", room);
                isWhite= false;
                break;
            default:
                break;
            }
        });
        client.set_close_listener([](sio::client::close_reason const &reason) {
            std::cout << "Disconnected from server" << std::endl;
        });
        client.connect(url);
    }

    // Set up Socket.IO event listeners.
    void setupListeners() {
        // Listen for board updates.
        client.socket()->on("board", [this](sio::event const &ev) {
            std::cout << "Received board update from server." << std::endl;
            updateGameStateFromJSON(ev.get_message(), gameState);
            isWhite ? moveHistoryWhite.push_back(gameState.hash) : moveHistoryBlack.push_back(gameState.hash);// Add new state to history
        });

        // Listen for turn notifications.
        client.socket()->on("turn", [this](sio::event const &ev) {
            std::cout << "It's our turn now." << std::endl;
            try {
                gameState.whiteToMove = isWhite;
				auto start = std::chrono::high_resolution_clock::now();
                Move bestMove = findBestMove(gameState, searchDepth,isWhite?moveHistoryWhite:moveHistoryBlack);
				auto end = std::chrono::high_resolution_clock::now();
				double duration_sec = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
                std::cout << "Computed best move: " << bestMove << std::endl;

                // Build JSON message to send the move.
                auto moveMsg = sio::object_message::create();
                moveMsg->get_map()["index"] = sio::string_message::create(boardMapping.at(bestMove.from));
                // Use the y–coordinate (from positions_indexes) to decide promotion.
                moveMsg->get_map()["x"] = sio::int_message::create(positions_indexes.at(bestMove.to).x);
                moveMsg->get_map()["y"] = sio::int_message::create(positions_indexes.at(bestMove.to).y);
                moveMsg->get_map()["king"] = sio::bool_message::create(isKing(positions_indexes.at(bestMove.from).y, gameState.whiteToMove));
                sio::message::list li;
                li.push(moveMsg);
                li.push(sio::int_message::create(isWhite ? 1 : 0));
                li.push(sio::double_message::create(duration_sec));

                client.socket()->emit("move piece", li);
            } catch (const std::exception &e) {
                std::cerr << "Error computing move: " << e.what() << std::endl;
            }
        });

        // Additional listeners (e.g., "msg", "rooms") can be added here.
    }
};

//////////////////////////////////////////////////////////////////////////////
// Main
//////////////////////////////////////////////////////////////////////////////

int main() {
    CheckersClient cc;
    cc.connectToServer("http://localhost:3001");
    cc.setupListeners();

    // Keep the client running.
    while (true) {
         std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    cc.client.sync_close();
    cc.client.clear_con_listeners();
    return 0;
}


