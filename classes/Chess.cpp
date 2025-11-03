#include "Chess.h"
#include <limits>
#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

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
    const char *wpieces = { "0PNBRQK" };
    const char *bpieces = { "0pnbrqk" };
    Bit *bit = _grid->getSquare(x, y)->bit();
    char notation = '0';
    if (bit) {
        notation = bit->gameTag() < 128 ? wpieces[bit->gameTag()] : bpieces[bit->gameTag()-128];
    }
    return notation;
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
    _gameOptions.rowX = 8;
    _gameOptions.rowY = 8;

    _grid->initializeChessSquares(pieceSize, "boardsquare.png");
    FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

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
    return true;
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
    _grid->forEachSquare([&](ChessSquare* square, int x, int y) {
        int index = y * 8 + x;
        char playerNumber = s[index] - '0';
        if (playerNumber) {
            square->setBit(PieceForPlayer(playerNumber - 1, Pawn));
        } else {
            square->setBit(nullptr);
        }
    });
}
