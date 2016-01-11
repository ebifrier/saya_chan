/*
  GodWhale, a  USI shogi(japanese-chess) playing engine derived from NanohaMini
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2010 Marco Costalba, Joona Kiiski, Tord Romstad (Stockfish author)
  Copyright (C) 2014 Kazuyuki Kawabata (NanohaMini author)
  Copyright (C) 2015 ebifrier, espelade, kakiage

  NanohaMini is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  GodWhale is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cassert>
#include <iostream>
#include <string>

#include "book.h"
#include "movegen.h"
#include "position.h"

Book *book;

Book::Book() : joseki(), RKiss()
{
    for (int i = abs(get_system_time() % 10000); i > 0; i--)
        RKiss.rand<unsigned>();
}
Book::~Book() {}
void Book::open(const std::string& fileName)
{
    FILE *fp = fopen(fileName.c_str(), "rb");
    if (fp == NULL) {
        perror(fileName.c_str());
        return;
    }

    BookKey key;
    BookEntry data;

    for (;;) {
        size_t n;
        n = fread(&key, sizeof(key), 1, fp);
        if (n == 0) break;
        n = fread(&data, sizeof(data), 1, fp);
        if (n == 0) break;
        Joseki_type::iterator p;
        p = joseki.find(key);
        if (p == joseki.end()) {
            joseki.insert(Joseki_value(key, data));
        } else {
            output_info("Error!:Duplicated opening data\n");
        }
    }

    fclose(fp);
}
void Book::close(){}
// 定跡データから、現在の局面kの合法手がその局面でどれくらいの頻度で指されたかを返す。
void Book::fromJoseki(Position &pos, int &mNum, MoveStack moves[], BookEntry data[])
{
    BookKey key;
    BookEntry d_null;
    MoveStack *last = generate<MV_LEGAL>(pos, moves);
    mNum = static_cast<int>(last - moves);
    memset(&d_null, 0, sizeof(d_null));

    int i;
    StateInfo newSt;
    for (i = 0; i < mNum; i++) {
        Move m = moves[i].move;
        pos.do_move(m, newSt);
        int ret = pos.EncodeHuffman(key.data);
        pos.undo_move(m);
        if (ret < 0) {
            pos.print_csa(m);
            output_info("Error!:Huffman encode!\n");
            continue;
        }
        Joseki_type::iterator p;
        p = joseki.find(key);
        if (p == joseki.end()) {
            // データなし
            data[i] = d_null;
        } else {
            // データあり
            data[i] = p->second;
        }
    }
}

// 現在の局面がどのくらいの頻度で指されたか定跡データを調べる
int Book::getHindo(const Position &pos)
{
    BookKey key;
    int hindo = 0;

    int ret = pos.EncodeHuffman(key.data);
    if (ret < 0) {
        pos.print_csa();
        output_info("Error!:Huffman encode!\n");
        return 0;
    }
    Joseki_type::iterator p;
    p = joseki.find(key);
    if (p != joseki.end()) {
        // データあり
        hindo = p->second.hindo;
    }

    return hindo;
}

Move Book::get_move(Position& pos, bool findBestMove)
{
    // 定跡データに手があればそれを使う.
    // 新形式を優先する
    if (size() > 0) {
        int teNum = 0;
        MoveStack moves[MAX_MOVES];
        BookEntry hindo2[MAX_MOVES];
        BookEntry candidate[4];
        int i;
        memset(candidate, 0, sizeof(candidate));

        fromJoseki(pos, teNum, moves, hindo2);
        // 一番勝率の高い手を選ぶ。
        float swin = 0.5f;
        float win_max = 0.0f;
        int max = -1;
        int max_hindo = -1;
        // 極端に少ない手は選択しない ⇒最頻出数の10分の1以下とする
        for (i = 0; i < teNum; i++) {
            if (max_hindo < hindo2[i].hindo) {
                max_hindo = hindo2[i].hindo;
            }
        }
        for (i = 0; i < teNum; i++) {
			//頻度4以下の手は候補に入れない
			if (hindo2[i].hindo > 4){
				//勝率算出
				if (hindo2[i].swin + hindo2[i].gwin > 0) {
					swin = hindo2[i].swin / float(hindo2[i].swin + hindo2[i].gwin);
					if (pos.side_to_move() != BLACK){ swin = 1.0f - swin; }
				}
				else {
					swin = 0.0f;
				}
				//勝率36%以下の手は候補に入れない
				if ( swin > 0.36f){
					// 多いものを候補に入れる(頻度順に並んだcandidateの適切な場所に挿入する）
					if (candidate[3].hindo < hindo2[i].hindo) {
						for (int j = 0; j < 4; j++) {
							if (candidate[j].hindo < hindo2[i].hindo) {
								for (int k = 3; k > j; k--) {
									candidate[k] = candidate[k-1];
								}
								candidate[j] = hindo2[i];
								candidate[j].eval = i;
								break;
							}
						}
					}
				}
			}
            // 極端に少ない手は選択しない ⇒最頻出数の10分の1以下とする
			if (max_hindo / 10 < hindo2[i].hindo && win_max < swin && hindo2[i].hindo > 4) {
				max = i;
				win_max = swin;
			}
        }

        if (max >= 0) {
            if (!findBestMove) {
                // 乱数で返す
                int n = Min(teNum, 4);
                int total = 0;
                for (i = 0; i < n; i++) {
                    total += candidate[i].hindo;
                }
                float p = RKiss.rand<unsigned short>() / 65536.0f;
                max = candidate[0].eval;
                int sum = 0;
                for (i = 0; i < n; i++) {
                    sum += candidate[i].hindo;
                    if (float(sum) / total >= p) {
                        max = candidate[i].eval;
                        break;
                    }
                }
                if (hindo2[max].swin + hindo2[max].gwin > 0) {
                    swin = hindo2[max].swin / float(hindo2[max].swin + hindo2[max].gwin);
					if (pos.side_to_move() != BLACK){ swin = 1.0f - swin; }
                } else {
                    swin = 0.0f;
                }
            }
            // もっともよさそうな手を返す
			if (swin > 0.36f){
				output_info("info string %5.1f%%, P(%d, %d)\n", swin*100.0f, hindo2[max].swin, hindo2[max].gwin);
				return moves[max].move;
			}
			else{
				output_info("info string No book data(plys=%d)\n", pos.startpos_ply_counter());
			}
        } else {
            output_info("info string No book data(plys=%d)\n", pos.startpos_ply_counter());
        }
    }

    return MOVE_NONE;
}

void makeBook(std::string& cmd){
	std::cout << "start makebook" << std::endl;
	std::string fileName;
	fileName = cmd;
	std::ifstream ifs(fileName.c_str(), std::ios::binary);
	if (!ifs) {
		std::cout << "I cannot open " << fileName << std::endl;
		return;
	}
	std::string line;

	const char* StarFEN = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
	std::map<BookKey,BookEntry> bookMap;

	long gameCount = 0;
	long entryCount = 0;

	while (std::getline(ifs, line)) {
		std::string elem;
		std::stringstream ss(line);
		ss >> elem; // 棋譜番号を飛ばす。
		std::cout << "kifu No."<<elem << std::endl;
		ss >> elem; // 対局日を飛ばす。
		ss >> elem; // 先手
		const std::string sente = elem;
		ss >> elem; // 後手
		const std::string gote = elem;
		ss >> elem; // (0:引き分け,1:先手の勝ち,2:後手の勝ち)
		const Color winner = (elem == "1" ? BLACK : elem == "2" ? WHITE : COLOR_NONE);
		const Color saveColor = winner;
		int moveCount=0;

		if (!std::getline(ifs, line)) {
			std::cout << "!!! header only !!!" << std::endl;
			return;
		}

		//盤面初期化
		Position pos(StarFEN, 0); // The root position
		StateInfo state;

		int gameMovetotal = line.length() / 6 ;
		for (int i = 0; i < gameMovetotal; ++i){
			if (moveCount > 80){ break; }
			std::string moveStrCSA = line.substr(0, 6);
			std::string sengo = (pos.side_to_move() == BLACK ? "+" : "-");
			moveStrCSA = sengo + moveStrCSA;
			const Move move = move_from_csa(pos, moveStrCSA);
			++moveCount;
			if (move == MOVE_NONE) {
				//pos.print();
				std::cout << "!! Illegal move = Count" << moveCount << ": " << moveStrCSA << " !!" << std::endl;
				break;
			}
			line.erase(0, 6); // 先頭から6文字削除
			//一手すすめる
			pos.do_move(move, state);

				BookKey key;
				int ret = pos.EncodeHuffman(key.data);
				if (ret < 0) {
					std::cout << std::endl<< gameCount << std::endl;
					pos.print_csa();
					output_info("Error!:Huffman encode!\n");
					break;
				}

				const Score score = SCORE_ZERO;
				//BookEntryの初期化
				BookEntry be;
				be.eval = score;
				be.hindo = 1;
				if (winner == BLACK){
					be.swin = 1; be.gwin = 0;
				}
				else{
					be.swin = 0; be.gwin = 1;
				}
				std::map<BookKey, BookEntry>::_Pairib pib = bookMap.insert(std::make_pair(key, be));
				if (!pib.second){//キーが重複
					auto it = pib.first;
					if ((*it).second.hindo < USHRT_MAX){
						++(*it).second.hindo;
						if (winner == BLACK){ ++(*it).second.swin; }
						else{ ++(*it).second.gwin; }
					}
				}
				else{/*局面登録成功*/
					++entryCount;
				}
		}
		++gameCount;
		//std::cout << "done:" << gameCount << std::endl;
		/*if (gameCount % 500 == 0){ std::cout << entryCount << std::endl; }
		else if (gameCount % 100 == 0){ std::cout << "o"; }
		else if (gameCount % 20 == 0){ std::cout << "."; }*/
	}

	std::cout << "file making..." << std::endl;

	std::ofstream ofs("book.jsk", std::ios::binary);
	for (auto& elem : bookMap) {
		BookKey bKey = elem.first;
		BookEntry bEntry = elem.second;
		ofs.write(reinterpret_cast<char*>(&(bKey)), sizeof(BookKey));
		ofs.write(reinterpret_cast<char*>(&(bEntry)), sizeof(BookEntry));
	}
	std::cout << "end makebook" << std::endl;
}
