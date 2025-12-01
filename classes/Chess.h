#pragma once

#include "Game.h"
#include "Grid.h"
#include "Bitboard.h"

constexpr int pieceSize = 80;

class Chess : public Game
{
public:
    Chess();
    ~Chess();

    void setUpBoard() override;

    bool canBitMoveFrom(Bit &bit, BitHolder &src) override;
    bool canBitMoveFromTo(Bit &bit, BitHolder &src, BitHolder &dst) override;
    bool actionForEmptyHolder(BitHolder &holder) override;

    void stopGame() override;

    Player *checkForWinner() override;
    bool checkForDraw() override;

    std::string initialStateString() override;
    std::string stateString() override;
    void setStateString(const std::string &s) override;

    Grid* getGrid() override { return _grid; }

    std::vector<BitMove> generateAllMoves(const std::string& state, int playerColor);

private:
    Bit* PieceForPlayer(const int playerNumber, ChessPiece piece);
    Player* ownerAt(int x, int y) const;
    void FENtoBoard(const std::string& fen);
    char pieceNotation(int x, int y) const;
    void generateKnightMoves(std::vector<BitMove>& moves, BitboardElement knightBoard, uint64_t emptyOrEnemy);
    void generateKingMoves(std::vector<BitMove>& moves, BitboardElement kingBoard, uint64_t emptyOrEnemy);
    void generatePawnMoves(std::vector<BitMove>& moves, int owner);

    Grid* _grid;
};