#include "Chess.h"
#include "Bitboard.h"
#include "GameState.h"
#include <limits>
#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cctype>
#include <map>
#include <algorithm>

static constexpr int NEG_INF = -1000000000;
static constexpr int POS_INF =  1000000000;
static constexpr int MATE_SCORE = 10'000'000;

static long long g_countNodes = 0;

static bool g_masksInit = false;

static inline bool onBoard(int x, int y) { return (x >= 0 && x < 8 && y >= 0 && y < 8); }
static inline int sqIndex(int x, int y) { return y * 8 + x; }

static inline int pieceTypeFromTag(int tag) {
    return (tag >= 128) ? (tag - 128) : tag;
}

static inline int ownerFromTag(int tag) { return (tag >= 128) ? 1 : 0; }

static int findKingSquare(const char state[64], char kingChar) {
    for (int i = 0; i < 64; ++i) {
        if (state[i] == kingChar) return i;
    }
    return -1;
}

static void buildGameStateFromBoard(Chess* self, GameState& gs, char color)
{
    std::string ui = self->stateString();
    char engineState[64];

    for (int uiIdx = 0; uiIdx < 64; ++uiIdx) {
        int file = uiIdx % 8;
        int rankFromTop = uiIdx / 8;
        int y = 7 - rankFromTop;
        int engineIdx = y * 8 + file;
        engineState[engineIdx] = ui[uiIdx];
    }

    gs.init(engineState, color);
}


Chess::Chess()
{
    _grid = new Grid(8, 8);
}

Chess::~Chess()
{
    delete _grid;
}

char Chess::pieceNotation(int x, int y) const
{
    const char* wpieces = "0PNBRQK";
    const char* bpieces = "0pnbrqk";

    Bit* bit = _grid->getSquare(x, y)->bit();
    if (!bit)
        return '0';

    int tag = bit->gameTag();
    bool isBlack = (tag >= 128);
    int ptype = isBlack ? (tag - 128) : tag;

    if (ptype < 0 || ptype > 6)
        return '0';

    return isBlack ? bpieces[ptype] : wpieces[ptype];
}

Bit* Chess::PieceForPlayer(const int playerNumber, ChessPiece piece)
{
    const char* pieces[] = { "pawn.png", "knight.png", "bishop.png", "rook.png", "queen.png", "king.png" };

    Bit* bit = new Bit();
    // should possibly be cached from player class?
    const char* pieceName = pieces[piece - 1];
    std::string spritePath = std::string("") + (playerNumber == 0 ? "w_" : "b_") + pieceName;
    bit->LoadTextureFromFile(spritePath.c_str());
    bit->setOwner(getPlayerAt(playerNumber));
    bit->setSize(pieceSize, pieceSize);

    return bit;
}

void Chess::setUpBoard()
{
    setNumberOfPlayers(2);
    setAIPlayer(1);
    _gameOptions.AIMAXDepth = 3;
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

    std::string state = stateString();

    startGame();
}

void Chess::FENtoBoard(const std::string& fen) {
    // convert a FEN string to a board
    // FEN is a space delimited string with 6 fields
    // 1: piece placement (from white's perspective)

    _grid->forEachSquare([](ChessSquare* sq, int, int) {
        sq->destroyBit();
    });

    const std::string placement = fen.substr(0, fen.find(' '));
    int x = 0;
    int y = 7;

    auto charToPiece = [](char u) -> ChessPiece {
        switch (u) {
            case 'P': return Pawn;
            case 'N': return Knight;
            case 'B': return Bishop;
            case 'R': return Rook;
            case 'Q': return Queen;
            case 'K': return King;
            default:  return Pawn;
        }
    };

    auto place = [&](int player, ChessPiece piece, int fx, int fy) {
        if (fx < 0 || fx >= 8 || fy < 0 || fy >= 8) return;

        ChessSquare* sq = _grid->getSquare(fx, fy);
        if (!sq) return;

        Bit* b = PieceForPlayer(player, piece);

        int tagBase = static_cast<int>(piece);
        b->setGameTag(player == 0 ? tagBase : (128 + tagBase));

        b->setPosition(sq->getPosition());
        sq->setBit(b);
    };

    for (char c : placement) {
        if (c == '/') {
            y--;
            x = 0;
            if (y > 7) break;
            continue;
        }
        if (c >= '1' && c <= '8') {
            x += (c - '0');
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c))) {
            int player = std::isupper(static_cast<unsigned char>(c)) ? 0 : 1;
            char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            place(player, charToPiece(u), x, y);
            x++;
        }
    }

    // NOT PART OF THIS ASSIGNMENT BUT OTHER THINGS THAT CAN BE IN A FEN STRING
    // ARE BELOW
    // 2: active color (W or B)
    // 3: castling availability (KQkq or -)
    // 4: en passant target square (in algebraic notation, or -)
    // 5: halfmove clock (number of halfmoves since the last capture or pawn advance)
}

bool Chess::actionForEmptyHolder(BitHolder &holder)
{
    return false;
}

bool Chess::canBitMoveFrom(Bit &bit, BitHolder &src)
{
    // need to implement friendly/unfriendly in bit so for now this hack
    int currentPlayer = getCurrentPlayer()->playerNumber() * 128;
    int pieceColor = bit.gameTag() & 128;
    if (pieceColor == currentPlayer) return true;
    return false;
}

bool Chess::canBitMoveFromTo(Bit& bit, BitHolder& src, BitHolder& dst)
{
    if (bit.getOwner() != getCurrentPlayer())
        return false;

    auto* s = static_cast<ChessSquare*>(&src);
    auto* d = static_cast<ChessSquare*>(&dst);

    int fromEngine = s->getRow() * 8 + s->getColumn();
    int toEngine   = d->getRow() * 8 + d->getColumn();

    std::string ui = stateString();
    char engineState[64];

    for (int uiIdx = 0; uiIdx < 64; ++uiIdx) {
        int file = uiIdx % 8;
        int rankFromTop = uiIdx / 8;
        int y = 7 - rankFromTop;
        int engineIdx = y * 8 + file;
        engineState[engineIdx] = ui[uiIdx];
    }

    int color = (getCurrentPlayer()->playerNumber() == 0) ? WHITE : BLACK;

    GameState gs;
    gs.init(engineState, (char)color);

    auto moves = gs.generateAllMoves();

    for (const auto& m : moves) {
        if ((int)m.from == fromEngine && (int)m.to == toEngine)
            return true;
    }

    return false;
}

void Chess::stopGame()
{
    _grid->forEachSquare([](ChessSquare* square, int x, int y) {
        square->destroyBit();
    });
}

Player* Chess::ownerAt(int x, int y) const
{
    if (x < 0 || x >= 8 || y < 0 || y >= 8) {
        return nullptr;
    }

    auto square = _grid->getSquare(x, y);
    if (!square || !square->bit()) {
        return nullptr;
    }
    return square->bit()->getOwner();
}

Player* Chess::checkForWinner()
{
    char color = (getCurrentPlayer()->playerNumber() == 0) ? WHITE : BLACK;

    GameState gs;
    buildGameStateFromBoard(this, gs, color);

    auto moves = gs.generateAllMoves();
    if (!moves.empty()) return nullptr;

    if (gs.inCheck(color)) {
        int winnerIndex = (getCurrentPlayer()->playerNumber() == 0) ? 1 : 0;
        return getPlayerAt(winnerIndex);
    }
    return nullptr;
}

bool Chess::checkForDraw()
{
    char color = (getCurrentPlayer()->playerNumber() == 0) ? WHITE : BLACK;

    GameState gs;
    buildGameStateFromBoard(this, gs, color);

    auto moves = gs.generateAllMoves();
    if (!moves.empty()) return false;

    return !gs.inCheck(color);
}

std::string Chess::initialStateString()
{
    return stateString();
}

std::string Chess::stateString()
{
    std::string s;
    s.reserve(64);
    for (int y = 7; y >= 0; --y) {
        for (int x = 0; x < 8; ++x) {
            s += pieceNotation(x, y);
        }
    }
    return s;
}

void Chess::setStateString(const std::string &s)
{
    if (s.size() < 64)
        return;

    _grid->forEachSquare([](ChessSquare* square, int, int) {
        square->destroyBit();
    });

    auto charToPiece = [](char u) -> ChessPiece {
        switch (u) {
            case 'P': return Pawn;
            case 'N': return Knight;
            case 'B': return Bishop;
            case 'R': return Rook;
            case 'Q': return Queen;
            case 'K': return King;
            default:  return Pawn;
        }
    };

    for (int idx = 0; idx < 64; ++idx)
    {
        char c = s[idx];
        if (c == '0')
            continue;

        int file = idx % 8;
        int rankFromTop = idx / 8;
        int x = file;
        int y = 7 - rankFromTop;

        int player = std::isupper(static_cast<unsigned char>(c)) ? 0 : 1;
        char u = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        ChessPiece piece = charToPiece(u);

        ChessSquare* sq = _grid->getSquare(x, y);
        if (!sq) continue;

        Bit* b = PieceForPlayer(player, piece);

        int tagBase = static_cast<int>(piece);
        b->setGameTag(player == 0 ? tagBase : (128 + tagBase));

        b->setPosition(sq->getPosition());
        sq->setBit(b);
    }
}

static inline bool isWhitePiece(char c)
{
    return std::isupper(static_cast<unsigned char>(c));
}

static inline void indexToFR(int idx, int& file, int& rank)
{
    rank = idx / 8;
    file = idx % 8;
}

static inline int FRToIndex(int file, int rank)
{
    return rank * 8 + file;
}

static inline void indexToXYGrid(int idx, int& x, int& y)
{
    int rankFromTop = idx / 8; 
    int file        = idx % 8; 

    x = file;
    y = 7 - rankFromTop;
}

static std::map<char, int> evaluateScores = {
    {'P',  100}, {'p', -100}, // Pawns
    {'N',  200}, {'n', -200}, // Knights
    {'B',  230}, {'b', -230}, // Bishops
    {'R',  400}, {'r', -400}, // Rooks
    {'Q',  900}, {'q', -900}, // Queens
    {'K', 2000}, {'k',-2000}, // Kings
    {'0',    0}               // Empty square
};

static int evaluateBoard(const std::string& state)
{
    int value = 0;
    for (char ch : state) {
        value += evaluateScores[ch];
    }
    return value;
}

static int evaluateBoard(const char st[64]) {
    int s = 0;
    for (int i = 0; i < 64; ++i) s += evaluateScores[st[i]];
    return s;
}

static int negamax(GameState& gs, int depth, int alpha, int beta)
{
    auto moves = gs.generateAllMoves();

    if (moves.empty())
    {
        if (gs.inCheck(gs.color)) {
            return -(MATE_SCORE + depth);
        }
        return 0;
    }

    if (depth == 0) {
        return evaluateBoard(gs.state) * gs.color;
    }

    int best = NEG_INF;

    for (const auto& m : moves) {
        gs.pushMove(m);
        int val = -negamax(gs, depth - 1, -beta, -alpha);
        gs.popState();

        if (val > best) best = val;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }

    return best;
}

void Chess::updateAI()
{
    Player* cur = getCurrentPlayer();
    if (!cur || !cur->isAIPlayer()) return;

    std::string ui = stateString();
    char engineState[64];

    for (int uiIdx = 0; uiIdx < 64; ++uiIdx) {
        int file = uiIdx % 8;
        int rankFromTop = uiIdx / 8;
        int y = 7 - rankFromTop;
        int engineIdx = y * 8 + file;
        engineState[engineIdx] = ui[uiIdx];
    }

    int color = (cur->playerNumber() == 0) ? WHITE : BLACK;

    GameState gs;
    gs.init(engineState, (char)color);

    int depth = (_gameOptions.AIMAXDepth > 0) ? _gameOptions.AIMAXDepth : 3;

    auto rootMoves = gs.generateAllMoves();
    if (rootMoves.empty()) return;

    int bestVal = NEG_INF;
    BitMove bestMove;

    for (const auto& m : rootMoves) {
        gs.pushMove(m);
        int val = -negamax(gs, depth - 1, NEG_INF, POS_INF);
        gs.popState();

        if (val > bestVal) {
            bestVal = val;
            bestMove = m;
        }
    }

    gs.pushMove(bestMove);

    std::string newUi(64, '0');
    for (int engineIdx = 0; engineIdx < 64; ++engineIdx) {
        int file = engineIdx % 8;
        int y = engineIdx / 8;
        int rankFromTop = 7 - y;
        int uiIdx = rankFromTop * 8 + file;
        newUi[uiIdx] = gs.state[engineIdx];
    }

    setStateString(newUi);
    endTurn();
}
