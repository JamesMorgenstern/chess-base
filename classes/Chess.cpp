#include "Chess.h"
#include "Bitboard.h"
#include <limits>
#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <cctype>
#include <map>

static constexpr int WHITE =  1;
static constexpr int BLACK = -1;

static constexpr int NEG_INF = -1'000'000'000;
static constexpr int POS_INF =  1'000'000'000;

static long long g_countNodes = 0;

// ---------- Bitboard helpers ----------
static uint64_t KNIGHT_MASKS[64];
static uint64_t KING_MASKS[64];
static bool g_masksInit = false;

static inline bool onBoard(int x, int y) { return (x >= 0 && x < 8 && y >= 0 && y < 8); }
static inline int sqIndex(int x, int y) { return y * 8 + x; }

static void initMoveMasks() {
    if (g_masksInit) return;
    const int kdx[8] = {+1,+2,+2,+1,-1,-2,-2,-1};
    const int kdy[8] = {+2,+1,-1,-2,-2,-1,+1,+2};
    const int Kdx[8] = {+1,+1, 0,-1,-1,-1, 0,+1};
    const int Kdy[8] = { 0,+1,+1,+1, 0,-1,-1,-1};

    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            int from = sqIndex(x, y);
            uint64_t nmask = 0, kmask = 0;

            for (int i = 0; i < 8; ++i) {
                int nx = x + kdx[i], ny = y + kdy[i];
                if (onBoard(nx, ny)) nmask |= (1ULL << sqIndex(nx, ny));
            }
            for (int i = 0; i < 8; ++i) {
                int nx = x + Kdx[i], ny = y + Kdy[i];
                if (onBoard(nx, ny)) kmask |= (1ULL << sqIndex(nx, ny));
            }

            KNIGHT_MASKS[from] = nmask;
            KING_MASKS[from]   = kmask;
        }
    }
    g_masksInit = true;
}

static void getOccupancy(Chess* self, int currentPlayer,
                         uint64_t& friendly, uint64_t& enemy) {
    friendly = enemy = 0ULL;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            auto sq = self->getGrid()->getSquare(x, y);
            if (!sq) continue;
            auto b = sq->bit();
            if (!b) continue;

            int tag = b->gameTag();
            int isBlack = (tag >= 128);
            int owner   = isBlack ? 1 : 0;

            uint64_t bit = (1ULL << sqIndex(x, y));
            if (owner == currentPlayer) friendly |= bit;
            else                        enemy    |= bit;
        }
    }
}

static inline int pieceTypeFromTag(int tag) {
    return (tag >= 128) ? (tag - 128) : tag;
}

static inline int ownerFromTag(int tag) { return (tag >= 128) ? 1 : 0; }

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
    std::vector<BitMove> debugMoves = generateAllMoves(state, 1);
    int moveCount = (int)debugMoves.size();

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

bool Chess::canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst)
{
    if (bit.getOwner() != getCurrentPlayer()) return false;

    auto* s = static_cast<ChessSquare*>(&src);
    auto* d = static_cast<ChessSquare*>(&dst);
    int sx = s->getColumn(), sy = s->getRow();
    int dx = d->getColumn(), dy = d->getRow();
    int from = sqIndex(sx, sy), to = sqIndex(dx, dy);

    int tag    = bit.gameTag();
    int owner  = ownerFromTag(tag);         
    int ptype  = pieceTypeFromTag(tag);

    uint64_t friendly=0, enemy=0;
    getOccupancy(this, owner, friendly, enemy);
    uint64_t empty   = ~(friendly | enemy);
    uint64_t emptyOrEnemy = ~friendly;

    if (dst.bit() && dst.bit()->getOwner() == bit.getOwner())
        return false;

    if (ptype == Knight) {
        initMoveMasks();
        uint64_t legal = KNIGHT_MASKS[from] & emptyOrEnemy;
        return (legal & (1ULL << to)) != 0ULL;
    }
    if (ptype == King) {
        initMoveMasks();
        uint64_t legal = KING_MASKS[from] & emptyOrEnemy;
        return (legal & (1ULL << to)) != 0ULL;
    }

    if (ptype == Pawn) {
        int dir  = (owner == 0) ? +1 : -1;
        int startRank = (owner == 0) ? 1 : 6;

        if (dx == sx && dy == sy + dir) {
            if (!dst.bit()) return true;
            return false;
        }
        if (dx == sx && dy == sy + 2*dir && sy == startRank) {
            int midy = sy + dir;
            if (!_grid->getSquare(sx, midy)->bit() && !dst.bit()) return true;
            return false;
        }
        if (dy == sy + dir && (dx == sx + 1 || dx == sx - 1)) {
            if (dst.bit() && dst.bit()->getOwner() != bit.getOwner()) return true;
            return false;
        }
        return false;
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
    return nullptr;
}

bool Chess::checkForDraw()
{
    return false;
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

void Chess::generateKnightMoves(std::vector<BitMove>& moves, BitboardElement knightBoard, uint64_t emptyOrEnemy) 
{
    initMoveMasks();

    knightBoard.forEachBit([&](int fromSquare)
    {
        uint64_t targets = KNIGHT_MASKS[fromSquare] & emptyOrEnemy;
        BitboardElement targetBB(targets);
        targetBB.forEachBit([&](int toSquare)
        {
            moves.emplace_back(fromSquare, toSquare, Knight);
        });
    });
}

void Chess::generateKingMoves(std::vector<BitMove>& moves, BitboardElement kingBoard, uint64_t emptyOrEnemy) 
{
    initMoveMasks();

    kingBoard.forEachBit([&](int fromSquare)
    {
        uint64_t targets = KING_MASKS[fromSquare] & emptyOrEnemy;
        BitboardElement targetBB(targets);
        targetBB.forEachBit([&](int toSquare)
        {
            moves.emplace_back(fromSquare, toSquare, King);
        });
    });
}

void Chess::generatePawnMoves(std::vector<BitMove>& moves, int owner) 
{
    int dir  = (owner == 0) ? +1 : -1;
    int startRank = (owner == 0) ? 1 : 6;

    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            auto sq = _grid->getSquare(x, y);
            auto b  = sq ? sq->bit() : nullptr;
            if (!b) continue;

            int tag = b->gameTag();
            if (ownerFromTag(tag) != owner) continue;
            if (pieceTypeFromTag(tag) != Pawn) continue;

            int from = sqIndex(x, y);

            int ny = y + dir;
            if (onBoard(x, ny) && !_grid->getSquare(x, ny)->bit())
            {
                moves.emplace_back(from, sqIndex(x, ny), Pawn);

                if (y == startRank)
                {
                    int ny2 = y + 2*dir;
                    if (onBoard(x, ny2) &&
                        !_grid->getSquare(x, ny2)->bit())
                        moves.emplace_back(from, sqIndex(x, ny2), Pawn);
                }
            }

            for (int dx : {-1, +1})
            {
                int nx = x + dx;
                int cy = y + dir;
                if (!onBoard(nx, cy)) continue;
                auto target = _grid->getSquare(nx, cy);
                if (target && target->bit())
                {
                    int ttag = target->bit()->gameTag();
                    if (ownerFromTag(ttag) != owner)
                    {
                        moves.emplace_back(from, sqIndex(nx, cy), Pawn);
                    }
                }
            }
        }
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

std::vector<BitMove> Chess::generateAllMoves(const std::string& state, int playerColor)
{
    std::vector<BitMove> moves;
    moves.reserve(32);

    const bool whiteToMove = (playerColor > 0);

    for (int i = 0; i < 64; ++i)
    {
        char c = state[i];
        if (c == '0') continue;

        bool pieceIsWhite = isWhitePiece(c);
        if (pieceIsWhite != whiteToMove) continue;
        int file, rank;
        indexToFR(i, file, rank);

        char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        // ---------------- KNIGHTS ----------------
        if (lower == 'n')
        {
            const int kdx[8] = {+1,+2,+2,+1,-1,-2,-2,-1};
            const int kdy[8] = {-2,-1,+1,+2,+2,+1,-1,-2};

            for (int k = 0; k < 8; ++k)
            {
                int nf = file + kdx[k];
                int nr = rank + kdy[k];
                if (nf < 0 || nf > 7 || nr < 0 || nr > 7) continue;

                int toIdx = FRToIndex(nf, nr);
                char target = state[toIdx];
                if (target == '0')
                {
                    moves.emplace_back(i, toIdx, Knight);
                } 
                else
                {
                    bool targetWhite = isWhitePiece(target);
                    if (targetWhite != pieceIsWhite)
                    {
                        moves.emplace_back(i, toIdx, Knight);
                    }
                }
            }
        }

        // ---------------- KING ----------------
        else if (lower == 'k')
        {
            for (int df = -1; df <= 1; ++df)
            {
                for (int dr = -1; dr <= 1; ++dr)
                {
                    if (df == 0 && dr == 0) continue;
                    int nf = file + df;
                    int nr = rank + dr;
                    if (nf < 0 || nf > 7 || nr < 0 || nr > 7) continue;

                    int toIdx = FRToIndex(nf, nr);
                    char target = state[toIdx];
                    if (target == '0')
                    {
                        moves.emplace_back(i, toIdx, King);
                    } 
                    else
                    {
                        bool targetWhite = isWhitePiece(target);
                        if (targetWhite != pieceIsWhite)
                        {
                            moves.emplace_back(i, toIdx, King);
                        }
                    }
                }
            }
        }

        // ---------------- PAWNS ----------------
        else if (lower == 'p')
        {
            int dir    = whiteToMove ? -1 : +1;
            int startRank = whiteToMove ? 6 : 1;

            int oneStepRank = rank + dir;
            if (oneStepRank >= 0 && oneStepRank <= 7)
            {
                int oneIdx = FRToIndex(file, oneStepRank);
                if (state[oneIdx] == '0') {
                    moves.emplace_back(i, oneIdx, Pawn);

                    int twoStepRank = rank + 2 * dir;
                    if (rank == startRank && twoStepRank >= 0 && twoStepRank <= 7)
                    {
                        int twoIdx = FRToIndex(file, twoStepRank);
                        if (state[twoIdx] == '0')
                        {
                            moves.emplace_back(i, twoIdx, Pawn);
                        }
                    }
                }
            }

            for (int df : {-1, +1})
            {
                int nf = file + df;
                int nr = rank + dir;
                if (nf < 0 || nf > 7 || nr < 0 || nr > 7) continue;
                int capIdx = FRToIndex(nf, nr);
                char target = state[capIdx];
                if (target != '0')
                {
                    bool targetWhite = isWhitePiece(target);
                    if (targetWhite != pieceIsWhite)
                    {
                        moves.emplace_back(i, capIdx, Pawn);
                    }
                }
            }
        }
    }

    return moves;
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

static int negamax(Chess& game, std::string& state, int depth, int alpha, int beta, int playerColor)
{
    g_countNodes++;

    // Leaf: static evaluation
    if (depth == 0)
    {
        return evaluateBoard(state) * playerColor;
    }

    // Generate moves for THIS position and THIS side
    auto moves = game.generateAllMoves(state, playerColor);

    if (moves.empty())
    {
        return evaluateBoard(state) * playerColor;
    }

    int bestVal = NEG_INF;

    for (auto move : moves)
    {
        // Save board
        char boardSave   = state[move.to];
        char pieceMoving = state[move.from];

        // Make move
        state[move.to]   = pieceMoving;
        state[move.from] = '0';

        // Recurse
        int val = -negamax(game, state, depth - 1, -beta, -alpha, -playerColor);

        // Undo move
        state[move.from] = pieceMoving;
        state[move.to]   = boardSave;

        if (val > bestVal)
        {
            bestVal = val;
        }

        alpha = std::max(alpha, bestVal);
        if (alpha >= beta)
        {
            // alpha-beta cutoff
            break;
        }
    }

    return bestVal;
}

void Chess::updateAI()
{
    Player* current = getCurrentPlayer();
    if (!current || !current->isAIPlayer())
    {
        return;
    }

    int playerIndex = current->playerNumber();
    int playerColor = (playerIndex == 0) ? WHITE : BLACK;

    std::string state = stateString();

    int depth = (_gameOptions.AIMAXDepth > 0) ? _gameOptions.AIMAXDepth : 3;

    g_countNodes = 0;

    auto rootMoves = generateAllMoves(state, playerColor);
    if (rootMoves.empty())
    {
        return;
    }

    int bestVal = NEG_INF;
    BitMove bestMove;

    for (auto move : rootMoves)
    {
        char boardSave   = state[move.to];
        char pieceMoving = state[move.from];

        state[move.to]   = pieceMoving;
        state[move.from] = '0';

        int val = -negamax(*this, state, depth - 1, NEG_INF, POS_INF, -playerColor);

        state[move.from] = pieceMoving;
        state[move.to]   = boardSave;

        if (val > bestVal)
        {
            bestVal  = val;
            bestMove = move;
        }
    }

    if (bestVal == NEG_INF)
    {
        return;
    }

    int srcSquare = bestMove.from;
    int dstSquare = bestMove.to;

    int srcX, srcY, dstX, dstY;
    indexToXYGrid(srcSquare, srcX, srcY);
    indexToXYGrid(dstSquare, dstX, dstY);

    std::string realState = stateString();
    realState[dstSquare] = realState[srcSquare];
    realState[srcSquare] = '0';

    setStateString(realState);

    Bit* movedBit = _grid->getSquare(dstX, dstY)->bit();

    bitMovedFromTo(*movedBit, *_grid->getSquare(srcX, srcY), *_grid->getSquare(dstX, dstY));

    std::cout << "AI (player " << playerIndex
              << ") searched " << g_countNodes
              << " nodes, bestVal = " << bestVal << std::endl;
}