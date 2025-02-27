
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#ifdef _MSC_VER
#  include <intrin.h>
#  define __builtin_popcount __popcnt
#endif

// We represent the 32 playable squares with a 32–bit bitboard.
using Bitboard = uint32_t;

enum MoveType {
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
  MoveType type;
  Move(uint8_t f, uint8_t t, MoveType mt = URMove) : from(f), to(t), type(mt) {}
};

std::ostream &operator<<(std::ostream &os, const Move &m) {
  os << "from:" << static_cast<int>(m.from) << " to:" << static_cast<int>(m.to);
  return os;
}

struct GameState {
  Bitboard white;
  Bitboard black;
  Bitboard kings;
  Bitboard empty;
  bool whiteToMove;

  GameState() : white(0), black(0), kings(0), empty(0), whiteToMove(true) {}
  GameState(Bitboard w, Bitboard b, Bitboard k, Bitboard e, bool wtm)
      : white(w), black(b), kings(k), empty(e), whiteToMove(wtm) {}

  GameState copy() const {
    return GameState(white, black, kings, empty, whiteToMove);
  }
  Bitboard occupied() const { return white | black; }
  void updateEmpty() { empty = ~(white | black) & 0xFFFFFFFF; }
};

// ────────────────────────────────────────────────────────────
//  Mapping between index [0,31] and board (row,col) coordinates.
//  The 32 playable squares are the dark squares of an 8×8 board.
//  For even rows (0,2,4,6): playable squares are at columns 1,3,5,7.
//  For odd rows (1,3,5,7): playable squares are at columns 0,2,4,6.
//  (Row 0 is printed at the top.)
// ────────────────────────────────────────────────────────────

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

// ────────────────────────────────────────────────────────────
//  Precomputed move arrays computed via coordinate arithmetic.
// ────────────────────────────────────────────────────────────

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
    // White normal moves (moving “up” decreases the row).
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
    // Black normal moves (moving “down” increases the row).
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

// ────────────────────────────────────────────────────────────
//  For king promotion we use these masks (indices 0–3 are row 0, 28–31 are row
//  7). In our initial setup white promotes on row 0 and black on row 7.
// ────────────────────────────────────────────────────────────

constexpr Bitboard WHITE_KING_ROW = 0xF0000000; // indices 28–31
constexpr Bitboard BLACK_KING_ROW = 0x0000000F; // indices 0–3

// ────────────────────────────────────────────────────────────
//  Print the game state as an 8×8 grid. (Only playable squares show pieces.)
// ────────────────────────────────────────────────────────────

void printGameState(const GameState &state) {
  std::cout << "  0 1 2 3 4 5 6 7\n";
  int bitIndex = 0;
  for (int row = 0; row < 8; row++) {
    std::cout << row << " ";
    for (int col = 0; col < 8; col++) {
      bool isPlayable = (row % 2 == 0) ? (col % 2 == 1) : (col % 2 == 0);
      if (isPlayable) {
        char cell = '.';
        if (state.white & (1u << bitIndex))
          cell = (state.kings & (1u << bitIndex)) ? 'W' : 'w';
        else if (state.black & (1u << bitIndex))
          cell = (state.kings & (1u << bitIndex)) ? 'B' : 'b';
        std::cout << cell;
        bitIndex++;
      } else {
        std::cout << " ";
      }
      if (col < 7)
        std::cout << " ";
    }
    std::cout << "\n";
  }
  std::cout << (state.whiteToMove ? "White" : "Black") << " to move\n";
}

// ────────────────────────────────────────────────────────────
//  A simple hash function for GameState.
//  Combines white, black, kings and whose turn it is.
// ────────────────────────────────────────────────────────────

std::size_t hashState(const GameState &state) {
  std::size_t hash = std::hash<Bitboard>()(state.white);
  hash = hash * 31 + std::hash<Bitboard>()(state.black);
  hash = hash * 31 + std::hash<Bitboard>()(state.kings);
  hash = hash * 31 + std::hash<bool>()(state.whiteToMove);
  return hash;
}

// ────────────────────────────────────────────────────────────
//  Revised capture detection: check each direction separately.
// ────────────────────────────────────────────────────────────

bool isCapturePossible(const GameState &state) {
  Bitboard pieces = state.whiteToMove ? state.white : state.black;
  Bitboard kings = pieces & state.kings;
  Bitboard men = pieces & ~state.kings;
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

// ────────────────────────────────────────────────────────────
//  Move generation: if a capture is available, only generate capture moves.
// ────────────────────────────────────────────────────────────

std::vector<Move> generateMoves(const GameState &state) {
  std::vector<Move> moves;
  Bitboard pieces = state.whiteToMove ? state.white : state.black;
  Bitboard kings = pieces & state.kings;
  Bitboard men = pieces & ~state.kings;
  Bitboard opponents = state.whiteToMove ? state.black : state.white;
  bool mustCapture = isCapturePossible(state);

  if (mustCapture) {
    for (int i = 0; i < 32; i++) {
      if (!(men & (1u << i)))
        continue;
      if (state.whiteToMove) {
        if ((moves_array.whiteManLeft[i] & opponents) &&
            (moves_array.whiteManCapturesLeft[i] & state.empty)) {
          uint8_t dest = std::countr_zero(moves_array.whiteManCapturesLeft[i] &
                                          state.empty);
          moves.emplace_back(i, dest, ULCapture);
        }
        if ((moves_array.whiteManRight[i] & opponents) &&
            (moves_array.whiteManCapturesRight[i] & state.empty)) {
          uint8_t dest = std::countr_zero(moves_array.whiteManCapturesRight[i] &
                                          state.empty);
          moves.emplace_back(i, dest, URCapture);
        }
      } else {
        if ((moves_array.blackManLeft[i] & opponents) &&
            (moves_array.blackManCapturesLeft[i] & state.empty)) {
          uint8_t dest = std::countr_zero(moves_array.blackManCapturesLeft[i] &
                                          state.empty);
          moves.emplace_back(i, dest, DLCapture);
        }
        if ((moves_array.blackManRight[i] & opponents) &&
            (moves_array.blackManCapturesRight[i] & state.empty)) {
          uint8_t dest = std::countr_zero(moves_array.blackManCapturesRight[i] &
                                          state.empty);
          moves.emplace_back(i, dest, DRCapture);
        }
      }
    }
    for (int i = 0; i < 32; i++) {
      if (!(kings & (1u << i)))
        continue;
      if ((moves_array.whiteManLeft[i] & opponents) &&
          (moves_array.kingCapturesUL[i] & state.empty)) {
        uint8_t dest =
            std::countr_zero(moves_array.kingCapturesUL[i] & state.empty);
        moves.emplace_back(i, dest, ULCapture);
      }
      if ((moves_array.whiteManRight[i] & opponents) &&
          (moves_array.kingCapturesUR[i] & state.empty)) {
        uint8_t dest =
            std::countr_zero(moves_array.kingCapturesUR[i] & state.empty);
        moves.emplace_back(i, dest, URCapture);
      }
      if ((moves_array.blackManLeft[i] & opponents) &&
          (moves_array.kingCapturesDL[i] & state.empty)) {
        uint8_t dest =
            std::countr_zero(moves_array.kingCapturesDL[i] & state.empty);
        moves.emplace_back(i, dest, DLCapture);
      }
      if ((moves_array.blackManRight[i] & opponents) &&
          (moves_array.kingCapturesDR[i] & state.empty)) {
        uint8_t dest =
            std::countr_zero(moves_array.kingCapturesDR[i] & state.empty);
        moves.emplace_back(i, dest, DRCapture);
      }
    }
  } else {
    for (int i = 0; i < 32; i++) {
      if (!(men & (1u << i)))
        continue;
      if (state.whiteToMove) {
        if (moves_array.whiteManLeft[i] & state.empty) {
          uint8_t dest =
              std::countr_zero(moves_array.whiteManLeft[i] & state.empty);
          moves.emplace_back(i, dest, ULMove);
        }
        if (moves_array.whiteManRight[i] & state.empty) {
          uint8_t dest =
              std::countr_zero(moves_array.whiteManRight[i] & state.empty);
          moves.emplace_back(i, dest, URMove);
        }
      } else {
        if (moves_array.blackManLeft[i] & state.empty) {
          uint8_t dest =
              std::countr_zero(moves_array.blackManLeft[i] & state.empty);
          moves.emplace_back(i, dest, DLMove);
        }
        if (moves_array.blackManRight[i] & state.empty) {
          uint8_t dest =
              std::countr_zero(moves_array.blackManRight[i] & state.empty);
          moves.emplace_back(i, dest, DRMove);
        }
      }
    }
    for (int i = 0; i < 32; i++) {
      if (!(kings & (1u << i)))
        continue;
      if (moves_array.whiteManLeft[i] & state.empty) {
        uint8_t dest =
            std::countr_zero(moves_array.whiteManLeft[i] & state.empty);
        moves.emplace_back(i, dest, ULMove);
      }
      if (moves_array.whiteManRight[i] & state.empty) {
        uint8_t dest =
            std::countr_zero(moves_array.whiteManRight[i] & state.empty);
        moves.emplace_back(i, dest, URMove);
      }
      if (moves_array.blackManLeft[i] & state.empty) {
        uint8_t dest =
            std::countr_zero(moves_array.blackManLeft[i] & state.empty);
        moves.emplace_back(i, dest, DLMove);
      }
      if (moves_array.blackManRight[i] & state.empty) {
        uint8_t dest =
            std::countr_zero(moves_array.blackManRight[i] & state.empty);
        moves.emplace_back(i, dest, DRMove);
      }
    }
  }
  return moves;
}

// ────────────────────────────────────────────────────────────
//  Apply a move. For captures, remove the jumped-over piece (computed as the
//  midpoint). White promotes upon reaching row 0; Black promotes upon reaching
//  row 7.
// ────────────────────────────────────────────────────────────

GameState applyMove(const GameState &state, const Move &move) {
  GameState newState = state.copy();
  Bitboard &currentPieces =
      newState.whiteToMove ? newState.white : newState.black;
  Bitboard &opponentPieces =
      newState.whiteToMove ? newState.black : newState.white;

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

// ────────────────────────────────────────────────────────────
//  Simple evaluation: material (men=1, kings=2).
// ────────────────────────────────────────────────────────────

int evaluateState(const GameState &state) {
  int whiteCount = __builtin_popcount(state.white);
  int blackCount = __builtin_popcount(state.black);
  int whiteKingCount = __builtin_popcount(state.white & state.kings);
  int blackKingCount = __builtin_popcount(state.black & state.kings);
  int whiteScore = whiteCount + whiteKingCount * 2;
  int blackScore = blackCount + blackKingCount * 2;
  return whiteScore - blackScore;
}

// ────────────────────────────────────────────────────────────
//  Minimax with alpha-beta pruning and repetition detection.
//  The history vector stores hashes of states encountered on the current
//  branch. If a state repeats, we treat it as a draw (evaluation 0).
// ────────────────────────────────────────────────────────────

int minimax(const GameState &state, int depth, int alpha, int beta,
            bool maximizingPlayer, std::vector<std::size_t> &history) {
  if (depth == 0)
    return evaluateState(state);

  std::size_t h = hashState(state);
  // If this state is already in the history, consider it a draw.
  if (std::find(history.begin(), history.end(), h) != history.end())
    return 0;

  history.push_back(h);

  std::vector<Move> moves = generateMoves(state);
  if (moves.empty()) {
    history.pop_back();
    return maximizingPlayer ? std::numeric_limits<int>::min()
                            : std::numeric_limits<int>::max();
  }

  int bestEval;
  if (maximizingPlayer) {
    bestEval = std::numeric_limits<int>::min();
    for (const Move &m : moves) {
      GameState child = applyMove(state, m);
      int eval = minimax(child, depth - 1, alpha, beta, false, history);
      bestEval = std::max(bestEval, eval);
      alpha = std::max(alpha, eval);
      if (beta <= alpha)
        break;
    }
  } else {
    bestEval = std::numeric_limits<int>::max();
    for (const Move &m : moves) {
      GameState child = applyMove(state, m);
      int eval = minimax(child, depth - 1, alpha, beta, true, history);
      bestEval = std::min(bestEval, eval);
      beta = std::min(beta, eval);
      if (beta <= alpha)
        break;
    }
  }

  history.pop_back();
  return bestEval;
}

Move findBestMove(const GameState &state, int depth) {
  std::vector<Move> moves = generateMoves(state);
  if (moves.empty())
    throw std::runtime_error("No legal moves available");
  Move bestMove = moves[0];
  int bestValue = state.whiteToMove ? std::numeric_limits<int>::min()
                                    : std::numeric_limits<int>::max();
  int alpha = std::numeric_limits<int>::min();
  int beta = std::numeric_limits<int>::max();
  bool maximizing = state.whiteToMove;
  std::vector<std::size_t> history; // History for repetition detection.

  for (const Move &m : moves) {
    GameState child = applyMove(state, m);
    int moveValue =
        minimax(child, depth - 1, alpha, beta, !maximizing, history);
    if (maximizing) {
      if (moveValue > bestValue) {
        bestValue = moveValue;
        bestMove = m;
      }
      alpha = std::max(alpha, bestValue);
    } else {
      if (moveValue < bestValue) {
        bestValue = moveValue;
        bestMove = m;
      }
      beta = std::min(beta, bestValue);
    }
  }
  return bestMove;
}

// ────────────────────────────────────────────────────────────
//  Main function.
//  Black pieces on indices 0–11 (top three rows).
//  White pieces on indices 20–31 (bottom three rows).
// ────────────────────────────────────────────────────────────

int main() {
  auto start = std::chrono::high_resolution_clock::now();
  GameState state;
  // Black pieces on indices 0–11.
  state.black = 0;
  for (int i = 0; i < 12; i++) {
    state.black |= (1u << i);
  }
  // White pieces on indices 20–31.
  state.white = 0;
  for (int i = 20; i < 32; i++) {
    state.white |= (1u << i);
  }
  state.updateEmpty();
  state.whiteToMove = true;

  std::cout << "Initial board state:\n";
  printGameState(state);

  int searchDepth = 0;
  do
  {
  std::cout << "enter search depth:";
  std::cin >> searchDepth;
  } while (isdigit( searchDepth));

  while (state.black != 0 ||
         state.white != 0) { // extended loop to observe potential cycles
    std::cout << "\nFinding best move for "
              << (state.whiteToMove ? "White" : "Black") << "...\n";
    try {
      Move best = findBestMove(state, searchDepth);
      std::cout << "Best move: " << best << "\n";
      state = applyMove(state, best);
      std::cout << "Board after move:\n";
      printGameState(state);
    } catch (const std::exception &e) {
      std::cout << "Game over: " << e.what() << "\n";
      break;
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  std::cout << "\nExecution time: " << duration.count() << " ms\n";
  return 0;
}
