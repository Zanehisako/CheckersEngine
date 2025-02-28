#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <array>
#include <limits>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <bit>
#include <cstddef>

// Include the Socket.IO client header (from socket.io-client-cpp)
#include <sio_client.h>

#ifdef _MSC_VER
#  include <intrin.h>
#  define __builtin_popcount __popcnt
#endif

using namespace std;

//////////////////////////////////////////////////////////////////////////////
// Checkers Engine Code
//////////////////////////////////////////////////////////////////////////////

// We represent the 32 playable squares using a 32-bit bitboard.
using Bitboard = uint32_t;

enum MoveType {
    URCapture,
    ULCapture,
    DRCapture,
    DLCapture,
    URMove,
    ULMove,
    DRMove,
    DLMove
};

struct Move {
    uint8_t from;
    uint8_t to;
    MoveType type;
    Move(uint8_t f, uint8_t t, MoveType mt = URMove)
        : from(f), to(t), type(mt) {}
};

ostream& operator<<(ostream &os, const Move &m) {
    os << "from:" << static_cast<int>(m.from)
       << " to:" << static_cast<int>(m.to);
    return os;
}

// Define a struct to hold x and y values
struct Point {
    uint8_t x;
    uint8_t y;
};

 std::unordered_map<uint8_t, Point> possitions_indexes= {
        {23, {2, 3}},
        {12, {1, 2}},
        {24, {2, 4}},
        {13, {1, 3}},
        {00, {0, 0}},
        {01, {0, 1}},
        {02, {0, 2}},
        {03, {0, 3}},
        {04, {0, 4}},
        {05, {0, 5}},
        {06, {0, 6}},
        {07, {0, 7}},
        {10, {1, 0}},
        {11, {1, 1}},
        {14, {1, 4}},
        {15, {1, 5}},
        {16, {1, 6}},
        {17, {1, 7}},
        {20, {2, 0}},
        {21, {2, 1}},
        {22, {2, 2}},
        {25, {2, 5}},
        {26, {2, 6}},
        {27, {2, 7}},
        {30, {3, 0}},
        {31, {3, 1}},
        {32, {3, 2}},
        {33, {3, 3}},
        {34, {3, 4}},
        {35, {3, 5}},
        {36, {3, 6}},
        {37, {3, 7}},
        {40, {4, 0}},
        {41, {4, 1}},
        {42, {4, 2}},
        {43, {4, 3}},
        {44, {4, 4}},
        {45, {4, 5}},
        {46, {4, 6}},
        {47, {4, 7}},
        {50, {5, 0}},
        {51, {5, 1}},
        {52, {5, 2}},
        {53, {5, 3}},
        {54, {5, 4}},
        {55, {5, 5}},
        {56, {5, 6}},
        {57, {5, 7}},
        {60, {6, 0}},
        {61, {6, 1}},
        {62, {6, 2}},
        {63, {6, 3}},
        {64, {6, 4}},
        {65, {6, 5}},
        {66, {6, 6}},
        {67, {6, 7}},
        {70, {7, 0}},
        {71, {7, 1}},
        {72, {7, 2}},
        {73, {7, 3}},
        {74, {7, 4}},
        {75, {7, 5}},
        {76, {7, 6}},
        {77, {7, 7}}
    };

struct GameState {
    Bitboard white; // Bitboard for white pieces
    Bitboard black; // Bitboard for black pieces
    Bitboard kings; // Bitboard for kings (both sides)
    Bitboard empty; // Computed empty squares
    bool whiteToMove; // True if white's turn, false if black's turn

    GameState() : white(0), black(0), kings(0), empty(0), whiteToMove(true) {}
    GameState(Bitboard w, Bitboard b, Bitboard k, Bitboard e, bool wtm)
        : white(w), black(b), kings(k), empty(e), whiteToMove(wtm) {}

    GameState copy() const { return GameState(white, black, kings, empty, whiteToMove); }
    Bitboard occupied() const { return white | black; }
    void updateEmpty() { empty = ~(white | black) & 0xFFFFFFFF; }
};

//────────────────────────────────────────────────────────────
// Mapping between board indices [0,31] and (row, col) coordinates on an 8×8 board.
// For even rows (0,2,4,6): playable squares are at columns 1,3,5,7.
// For odd rows (1,3,5,7): playable squares are at columns 0,2,4,6.
// (Row 0 is printed at the top.)
//────────────────────────────────────────────────────────────

constexpr int indexFromRC(int row, int col) {
    if (row < 0 || row >= 8 || col < 0 || col >= 8)
        return -1;
    if (row % 2 == 0) { // even row: playable squares at odd columns
        if (col % 2 == 0)
            return -1;
        return row * 4 + ((col - 1) / 2);
    } else { // odd row: playable squares at even columns
        if (col % 2 != 0)
            return -1;
        return row * 4 + (col / 2);
    }
}

constexpr void rcFromIndex(int i, int &row, int &col) {
    row = i / 4;
    int j = i % 4;
    if (row % 2 == 0)
        col = 2 * j + 1;
    else
        col = 2 * j;
}

//────────────────────────────────────────────────────────────
// Precomputed move arrays (for normal moves and captures) computed via coordinate arithmetic.
//────────────────────────────────────────────────────────────

struct MoveArrays {
    std::array<Bitboard, 32> whiteManLeft{};
    std::array<Bitboard, 32> whiteManRight{};
    std::array<Bitboard, 32> whiteManCapturesLeft{};
    std::array<Bitboard, 32> whiteManCapturesRight{};
    std::array<Bitboard, 32> blackManLeft{};
    std::array<Bitboard, 32> blackManRight{};
    std::array<Bitboard, 32> blackManCapturesLeft{};
    std::array<Bitboard, 32> blackManCapturesRight{};
    std::array<Bitboard, 32> kingMoves{};
    std::array<Bitboard, 32> kingCapturesUL{};
    std::array<Bitboard, 32> kingCapturesUR{};
    std::array<Bitboard, 32> kingCapturesDL{};
    std::array<Bitboard, 32> kingCapturesDR{};
};

consteval MoveArrays initMoveArrays() {
    MoveArrays moves{};
    for (int i = 0; i < 32; i++) {
        int r, c;
        rcFromIndex(i, r, c);
        // White normal moves: moving "up" (decreasing row).
        if (r - 1 >= 0) {
            int dest = indexFromRC(r - 1, c - 1);
            if (dest != -1)
                moves.whiteManLeft[i] = 1u << dest;
            dest = indexFromRC(r - 1, c + 1);
            if (dest != -1)
                moves.whiteManRight[i] = 1u << dest;
        }
        // White captures: two steps up.
        if (r - 2 >= 0) {
            int dest = indexFromRC(r - 2, c - 2);
            if (dest != -1)
                moves.whiteManCapturesLeft[i] = 1u << dest;
            dest = indexFromRC(r - 2, c + 2);
            if (dest != -1)
                moves.whiteManCapturesRight[i] = 1u << dest;
        }
        // Black normal moves: moving "down" (increasing row).
        if (r + 1 < 8) {
            int dest = indexFromRC(r + 1, c - 1);
            if (dest != -1)
                moves.blackManLeft[i] = 1u << dest;
            dest = indexFromRC(r + 1, c + 1);
            if (dest != -1)
                moves.blackManRight[i] = 1u << dest;
        }
        // Black captures: two steps down.
        if (r + 2 < 8) {
            int dest = indexFromRC(r + 2, c - 2);
            if (dest != -1)
                moves.blackManCapturesLeft[i] = 1u << dest;
            dest = indexFromRC(r + 2, c + 2);
            if (dest != -1)
                moves.blackManCapturesRight[i] = 1u << dest;
        }
        moves.kingMoves[i] = moves.whiteManLeft[i] | moves.whiteManRight[i] |
                             moves.blackManLeft[i] | moves.blackManRight[i];
        moves.kingCapturesUL[i] = moves.whiteManCapturesLeft[i];
        moves.kingCapturesUR[i] = moves.whiteManCapturesRight[i];
        moves.kingCapturesDL[i] = moves.blackManCapturesLeft[i];
        moves.kingCapturesDR[i] = moves.blackManCapturesRight[i];
    }
    return moves;
}

constexpr auto moves_array = initMoveArrays();

//────────────────────────────────────────────────────────────
// For king promotion, we use these masks.
// (Here, for example, we assume that white promotes when reaching row 0 and black when reaching row 7.)
constexpr Bitboard WHITE_KING_ROW = 0xF0000000; // indices 28-31 (bottom row)
constexpr Bitboard BLACK_KING_ROW = 0x0000000F; // indices 0-3 (top row)

//────────────────────────────────────────────────────────────
// Print the board (8×8) using our bitboard representation.
// Only playable squares (dark squares) show a piece.
// 'w' and 'W' denote white pieces (W for king); 'b' and 'B' for black.
//────────────────────────────────────────────────────────────

void printGameState(const GameState &state) {
    cout << "  0 1 2 3 4 5 6 7\n";
    int bitIndex = 0;
    for (int row = 0; row < 8; row++) {
        cout << row << " ";
        for (int col = 0; col < 8; col++) {
            bool isPlayable = (row % 2 == 0) ? (col % 2 == 1) : (col % 2 == 0);
            if (isPlayable) {
                char cell = '.';
                if (state.white & (1u << bitIndex))
                    cell = (state.kings & (1u << bitIndex)) ? 'W' : 'w';
                else if (state.black & (1u << bitIndex))
                    cell = (state.kings & (1u << bitIndex)) ? 'B' : 'b';
                cout << cell;
                bitIndex++;
            } else {
                cout << " ";
            }
            if (col < 7)
                cout << " ";
        }
        cout << "\n";
    }
    cout << (state.whiteToMove ? "White" : "Black") << " to move\n";
}

//────────────────────────────────────────────────────────────
// Simple hash function for repetition detection.
std::size_t hashState(const GameState &state) {
    std::size_t hash = std::hash<Bitboard>()(state.white);
    hash = hash * 31 + std::hash<Bitboard>()(state.black);
    hash = hash * 31 + std::hash<Bitboard>()(state.kings);
    hash = hash * 31 + std::hash<bool>()(state.whiteToMove);
    return hash;
}

//────────────────────────────────────────────────────────────
// Move generation functions.
// If a capture is available, it is mandatory.
//────────────────────────────────────────────────────────────

bool isCapturePossible(const GameState &state) {
    Bitboard pieces = state.whiteToMove ? state.white : state.black;
    Bitboard kings  = pieces & state.kings;
    Bitboard men    = pieces & ~state.kings;
    Bitboard opponents = state.whiteToMove ? state.black : state.white;
    
    for (int i = 0; i < 32; i++) {
        if (men & (1u << i)) {
            if (state.whiteToMove) {
                if ((moves_array.whiteManLeft[i] & opponents) &&
                    (moves_array.whiteManCapturesLeft[i] & state.empty))
                    return true;
                if ((moves_array.whiteManRight[i] & opponents) &&
                    (moves_array.whiteManCapturesRight[i] & state.empty))
                    return true;
            } else {
                if ((moves_array.blackManLeft[i] & opponents) &&
                    (moves_array.blackManCapturesLeft[i] & state.empty))
                    return true;
                if ((moves_array.blackManRight[i] & opponents) &&
                    (moves_array.blackManCapturesRight[i] & state.empty))
                    return true;
            }
        }
    }
    for (int i = 0; i < 32; i++) {
        if (kings & (1u << i)) {
            if ((moves_array.whiteManLeft[i] & opponents) &&
                (moves_array.kingCapturesUL[i] & state.empty))
                return true;
            if ((moves_array.whiteManRight[i] & opponents) &&
                (moves_array.kingCapturesUR[i] & state.empty))
                return true;
            if ((moves_array.blackManLeft[i] & opponents) &&
                (moves_array.kingCapturesDL[i] & state.empty))
                return true;
            if ((moves_array.blackManRight[i] & opponents) &&
                (moves_array.kingCapturesDR[i] & state.empty))
                return true;
        }
    }
    return false;
}

std::vector<Move> generateMoves(const GameState &state) {
    vector<Move> moves;
    Bitboard pieces = state.whiteToMove ? state.white : state.black;
    Bitboard kings  = pieces & state.kings;
    Bitboard men    = pieces & ~state.kings;
    Bitboard opponents = state.whiteToMove ? state.black : state.white;
    bool mustCapture = isCapturePossible(state);
    
    if (mustCapture) {
        for (int i = 0; i < 32; i++) {
            if (!(men & (1u << i))) continue;
            if (state.whiteToMove) {
                if ((moves_array.whiteManLeft[i] & opponents) &&
                    (moves_array.whiteManCapturesLeft[i] & state.empty)) {
                    uint8_t dest = std::countr_zero(moves_array.whiteManCapturesLeft[i] & state.empty);
                    moves.emplace_back(i, dest, ULCapture);
                }
                if ((moves_array.whiteManRight[i] & opponents) &&
                    (moves_array.whiteManCapturesRight[i] & state.empty)) {
                    uint8_t dest = std::countr_zero(moves_array.whiteManCapturesRight[i] & state.empty);
                    moves.emplace_back(i, dest, URCapture);
                }
            } else {
                if ((moves_array.blackManLeft[i] & opponents) &&
                    (moves_array.blackManCapturesLeft[i] & state.empty)) {
                    uint8_t dest = std::countr_zero(moves_array.blackManCapturesLeft[i] & state.empty);
                    moves.emplace_back(i, dest, DLCapture);
                }
                if ((moves_array.blackManRight[i] & opponents) &&
                    (moves_array.blackManCapturesRight[i] & state.empty)) {
                    uint8_t dest = std::countr_zero(moves_array.blackManCapturesRight[i] & state.empty);
                    moves.emplace_back(i, dest, DRCapture);
                }
            }
        }
        for (int i = 0; i < 32; i++) {
            if (!(kings & (1u << i))) continue;
            if ((moves_array.whiteManLeft[i] & opponents) &&
                (moves_array.kingCapturesUL[i] & state.empty)) {
                uint8_t dest = std::countr_zero(moves_array.kingCapturesUL[i] & state.empty);
                moves.emplace_back(i, dest, ULCapture);
            }
            if ((moves_array.whiteManRight[i] & opponents) &&
                (moves_array.kingCapturesUR[i] & state.empty)) {
                uint8_t dest = std::countr_zero(moves_array.kingCapturesUR[i] & state.empty);
                moves.emplace_back(i, dest, URCapture);
            }
            if ((moves_array.blackManLeft[i] & opponents) &&
                (moves_array.kingCapturesDL[i] & state.empty)) {
                uint8_t dest = std::countr_zero(moves_array.kingCapturesDL[i] & state.empty);
                moves.emplace_back(i, dest, DLCapture);
            }
            if ((moves_array.blackManRight[i] & opponents) &&
                (moves_array.kingCapturesDR[i] & state.empty)) {
                uint8_t dest = std::countr_zero(moves_array.kingCapturesDR[i] & state.empty);
                moves.emplace_back(i, dest, DRCapture);
            }
        }
    } else {
        for (int i = 0; i < 32; i++) {
            if (!(men & (1u << i))) continue;
            if (state.whiteToMove) {
                if (moves_array.whiteManLeft[i] & state.empty) {
                    uint8_t dest = std::countr_zero(moves_array.whiteManLeft[i] & state.empty);
                    moves.emplace_back(i, dest, ULMove);
                }
                if (moves_array.whiteManRight[i] & state.empty) {
                    uint8_t dest = std::countr_zero(moves_array.whiteManRight[i] & state.empty);
                    moves.emplace_back(i, dest, URMove);
                }
            } else {
                if (moves_array.blackManLeft[i] & state.empty) {
                    uint8_t dest = std::countr_zero(moves_array.blackManLeft[i] & state.empty);
                    moves.emplace_back(i, dest, DLMove);
                }
                if (moves_array.blackManRight[i] & state.empty) {
                    uint8_t dest = std::countr_zero(moves_array.blackManRight[i] & state.empty);
                    moves.emplace_back(i, dest, DRMove);
                }
            }
        }
        for (int i = 0; i < 32; i++) {
            if (!(kings & (1u << i))) continue;
            if (moves_array.whiteManLeft[i] & state.empty) {
                uint8_t dest = std::countr_zero(moves_array.whiteManLeft[i] & state.empty);
                moves.emplace_back(i, dest, ULMove);
            }
            if (moves_array.whiteManRight[i] & state.empty) {
                uint8_t dest = std::countr_zero(moves_array.whiteManRight[i] & state.empty);
                moves.emplace_back(i, dest, URMove);
            }
            if (moves_array.blackManLeft[i] & state.empty) {
                uint8_t dest = std::countr_zero(moves_array.blackManLeft[i] & state.empty);
                moves.emplace_back(i, dest, DLMove);
            }
            if (moves_array.blackManRight[i] & state.empty) {
                uint8_t dest = std::countr_zero(moves_array.blackManRight[i] & state.empty);
                moves.emplace_back(i, dest, DRMove);
            }
        }
    }
    return moves;
}

GameState applyMove(const GameState &state, const Move &move) {
    GameState newState = state.copy();
    Bitboard &currentPieces = newState.whiteToMove ? newState.white : newState.black;
    Bitboard &opponentPieces = newState.whiteToMove ? newState.black : newState.white;

    currentPieces &= ~(1u << move.from);
    currentPieces |= (1u << move.to);
    if (newState.kings & (1u << move.from)) {
        newState.kings &= ~(1u << move.from);
        newState.kings |= (1u << move.to);
    }
    if (move.type == ULCapture || move.type == URCapture ||
        move.type == DLCapture || move.type == DRCapture) {
        int fromRow, fromCol, toRow, toCol;
        rcFromIndex(move.from, fromRow, fromCol);
        rcFromIndex(move.to, toRow, toCol);
        int capRow = (fromRow + toRow) / 2;
        int capCol = (fromCol + toCol) / 2;
        int capturedPos = indexFromRC(capRow, capCol);
        if (capturedPos != -1) {
            opponentPieces &= ~(1u << capturedPos);
            if (newState.kings & (1u << capturedPos))
                newState.kings &= ~(1u << capturedPos);
        }
    }
    int toRow, toCol;
    rcFromIndex(move.to, toRow, toCol);
    if (newState.whiteToMove && toRow == 0)
        newState.kings |= (1u << move.to);
    if (!newState.whiteToMove && toRow == 7)
        newState.kings |= (1u << move.to);

    newState.updateEmpty();
    newState.whiteToMove = !newState.whiteToMove;
    return newState;
}

int evaluateState(const GameState &state) {
    int whiteCount = __builtin_popcount(state.white);
    int blackCount = __builtin_popcount(state.black);
    int whiteKingCount = __builtin_popcount(state.white & state.kings);
    int blackKingCount = __builtin_popcount(state.black & state.kings);
    int whiteScore = whiteCount + whiteKingCount * 2;
    int blackScore = blackCount + blackKingCount * 2;
    return whiteScore - blackScore;
}

int minimax(const GameState &state, int depth, int alpha, int beta, bool maximizingPlayer, vector<size_t> &history) {
    if (depth == 0)
        return evaluateState(state);

    size_t h = hashState(state);
    if (find(history.begin(), history.end(), h) != history.end())
        return 0;
    history.push_back(h);

    vector<Move> moves = generateMoves(state);
    if (moves.empty()) {
        history.pop_back();
        return maximizingPlayer ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    }

    int bestEval;
    if (maximizingPlayer) {
        bestEval = std::numeric_limits<int>::min();
        for (const Move &m : moves) {
            GameState child = applyMove(state, m);
            int eval = minimax(child, depth - 1, alpha, beta, false, history);
            bestEval = max(bestEval, eval);
            alpha = max(alpha, eval);
            if (beta <= alpha)
                break;
        }
    } else {
        bestEval = std::numeric_limits<int>::max();
        for (const Move &m : moves) {
            GameState child = applyMove(state, m);
            int eval = minimax(child, depth - 1, alpha, beta, true, history);
            bestEval = min(bestEval, eval);
            beta = min(beta, eval);
            if (beta <= alpha)
                break;
        }
    }
    history.pop_back();
    return bestEval;
}

Move findBestMove(const GameState &state, int depth) {
    vector<Move> moves = generateMoves(state);
    if (moves.empty())
        throw runtime_error("No legal moves available");
    Move bestMove = moves[0];
    int bestValue = state.whiteToMove ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    int alpha = std::numeric_limits<int>::min();
    int beta = std::numeric_limits<int>::max();
    bool maximizing = state.whiteToMove;
    vector<size_t> history;

    for (const Move &m : moves) {
        GameState child = applyMove(state, m);
        int moveValue = minimax(child, depth - 1, alpha, beta, !maximizing, history);
        if (maximizing) {
            if (moveValue > bestValue) {
                bestValue = moveValue;
                bestMove = m;
            }
            alpha = max(alpha, bestValue);
        } else {
            if (moveValue < bestValue) {
                bestValue = moveValue;
                bestMove = m;
            }
            beta = min(beta, bestValue);
        }
    }
    return bestMove;
}

inline bool isKing(uint8_t x,bool white) {
    switch (white)
    {
    case true:
    if (x==7)
    {
        return true;
    }
    return false;

    case false:
    if (x==0)
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
// We assume that board[0] holds white pieces (top rows) and board[1] holds black pieces (bottom rows).
// Our engine expects black pieces on the top (indices 0–11) and white pieces on the bottom (indices 20–31).
// Thus, we swap the arrays when updating our GameState.
void updateGameStateFromJSON(const sio::message::ptr &msg, GameState &state) {
    // Reset the state.
    state.white = 0;
    state.black = 0;
    state.kings = 0;

    if (msg->get_flag() != sio::message::flag_array) {
        cerr << "Board update message is not an array!" << endl;
        return;
    }
    std::vector<sio::message::ptr> boardArr = msg->get_vector();
    if (boardArr.size()< 2) {
        cerr << "Board update does not contain two arrays." << endl;
        return;
    }

    // According to the server:
    // board[0] holds white pieces (top rows),
    // board[1] holds black pieces (bottom rows).
    // Our engine: state.black = pieces on top, state.white = pieces on bottom.
    // So assign board[0] to state.black and board[1] to state.white.
    auto arr0 = boardArr.at(1)->get_vector();
    for (auto &pieceMsg : arr0) {
        if (pieceMsg->get_flag() != sio::message::flag_object) continue;
        auto m = pieceMsg->get_map();
        int x = m["x"]->get_int();
        int y = m["y"]->get_int();
        bool king = m["king"]->get_bool();
        int idx = indexFromRC(y, x);
        if (idx >= 0) {
            state.black |= (1u << idx);
            if (king)
                state.kings |= (1u << idx);
        }
    }
    auto arr1 = boardArr.at(0)->get_vector();
    for (auto &pieceMsg : arr1) {
        if (pieceMsg->get_flag() != sio::message::flag_object) continue;
        auto m = pieceMsg->get_map();
        int x = m["x"]->get_int();
        int y = m["y"]->get_int();
        bool king = m["king"]->get_bool();
        int idx = indexFromRC(y, x);
        if (idx >= 0) {
            state.white |= (1u << idx);
            if (king)
                state.kings |= (1u << idx);
        }
    }
    state.updateEmpty();
    cout << "GameState updated from board JSON." << endl;
    printGameState(state);
}

//────────────────────────────────────────────────────────────
// CheckersClient class that integrates the engine with the Socket.IO server.
class CheckersClient {
public:
    sio::client client;
    GameState gameState;

    CheckersClient() {
        // Optionally initialize gameState here.
    }

    // Connect to the Socket.IO server.
    void connectToServer(const std::string &url) {
        client.set_open_listener([this]() {
            cout << "Connected to server" << endl;
            // Example: join room "room1"
            int choise;
            std::cout << "1: create room :\n2:join room:\n";
            std::cin >> choise;
            std::string room;
            std::cout << "Enter room name:\n";
            std::cin >> room;
            std::cout <<"choise:" << choise << std::endl;
            switch (choise)
            {
            case 1:
                std::cout << "creating a room\n";
                    client.socket()->emit("create room", room);
                    //gameState.whiteToMove = true;
                    break;
            case 2:
                std::cout << "joining as a player\n";
                    client.socket()->emit("join room as player", room);
                    //gameState.whiteToMove = false;
                    break;
            default:
                break;
            }
        });
        client.set_close_listener([](sio::client::close_reason const &reason) {
            cout << "Disconnected from server" << endl;
        });
        client.connect(url);
    }

    // Set up Socket.IO event listeners.
    void setupListeners() {
        // Listen for board updates.
        client.socket()->on("board", [this](sio::event const &ev) {
    cout << "Received board update from server." << endl;
    updateGameStateFromJSON(ev.get_message(), gameState);
});

        // Listen for turn notifications.
        client.socket()->on("turn", [this](sio::event const &ev) {
    cout << "It's our turn now." << endl;
    try {
        int searchDepth = 5; // adjust as needed
        Move bestMove = findBestMove(gameState, searchDepth);
        cout << "Computed best move: " << bestMove << endl;

        // Build JSON message to send the move.
        auto moveMsg = sio::object_message::create();
        moveMsg->get_map()["index"] = sio::string_message::create(std::to_string(bestMove.from));
        moveMsg->get_map()["x"] = sio::int_message::create(possitions_indexes.at(bestMove.to).x);
        moveMsg->get_map()["y"] = sio::int_message::create(possitions_indexes.at(bestMove.to).y);
        moveMsg->get_map()["king"] = sio::bool_message::create(isKing(possitions_indexes.at(bestMove.to).x,gameState.whiteToMove));
        sio::message::list li;
        li.push(moveMsg);
        li.push(sio::int_message::create(gameState.whiteToMove?0:1));
        li.push(sio::int_message::create(1));

        client.socket()->emit("move piece", li);
    } catch (const std::exception &e) {
        cerr << "Error computing move: " << e.what() << endl;
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
    // Connect to your Socket.IO server.
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


