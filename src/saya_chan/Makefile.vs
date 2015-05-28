#
# なのはminiにするときはNANOHAMINI=1の前の#を取り、
# なのはmicroにするときはNANOHAMINI=1の前に#を付け、NANOHAMICRO=1の前の#を取る
#
NANOHAMINI=1
#NANOHAMICRO=1

!IFDEF NANOHAMINI
EVAL_TYPE=EVAL_OLD
EXE = nanohamini.exe
PGD = nanohamini.pgd
PGOLOG = nanohamini_prof.txt
!ELSEIFDEF NANOHAMICRO
EVAL_TYPE=EVAL_MICRO
EXE = nanohamicro.exe
PGD = nanohamicro.pgd
PGOLOG = nanohamicro_prof.txt
!ELSE
!ERROR undefined eval_type
!ENDIF

OBJS = mate1ply.obj misc.obj timeman.obj evaluate.obj position.obj \
	 tt.obj main.obj move.obj \
	 movegen.obj search.obj uci.obj movepick.obj thread.obj ucioption.obj \
	 benchmark.obj book.obj \
	 shogi.obj mate.obj problem.obj

CC=cl
LD=link



# Compile Options
#
# -DEVAL_OLD        なのはmini(2駒関係の評価関数)
# -DEVAL_MICRO      なのはmicro(駒得のみの評価関数)
#
# Visual C++オプション
#
# /D_CRT_SECURE_NO_WARNINGS
#                   secureな関数を使っていないときの警告を出さない
# /Zc:forScope      スコープ ループに標準 C++ を適用する
# /Wall             警告をすべて有効にする
# /GS[-]            セキュリティ チェックを有効にする
# /favor:<blend|AMD64|EM64T> 最適化するプロセッサ
# /GL[-]            リンク時のコード生成を行う
# /RTCs             スタック フレーム ランタイム チェック
# /RTCu             初期化されていないローカル変数のチェック

FLAGS = -DNDEBUG -D$(EVAL_TYPE) -DNANOHA -DCHK_PERFORM  \
	-DOLD_LOCKS /favor:AMD64 /EHsc /D_CRT_SECURE_NO_WARNINGS \
	 /GL /Zc:forScope
#CXXFLAGS=$(FLAGS) /MT /W4 /Wall /nologo /Od /GS /RTCsu
CXXFLAGS=$(FLAGS) /MD /W3 /nologo /Ox /Ob2 /GS- /Gm /Zi
LDFLAGS=/NOLOGO /STACK:16777216,32768 /out:$(EXE) /LTCG /DEBUG
PGOLDFLAGS1=/NOLOGO /STACK:16777216,32768 /out:$(EXE) /LTCG:PGI
PGOLDFLAGS2=/NOLOGO /STACK:16777216,32768 /out:$(EXE) /LTCG:PGO


all: $(EXE)

$(EXE) : $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) User32.lib

.cpp.obj :
	$(CC) $(CXXFLAGS) /c $*.cpp

clean :
	del /q *.obj
	del /q *.idb
	del /q *.pdb
	del /q *.pgc
	del /q *.pgd
	del /q *.suo
	del    $(PGOLOG)
	del    $(EXE)

pgo: $(OBJS)
	$(LD) $(PGOLDFLAGS1) $(OBJS) User32.lib
	$(EXE) bench 128 2 9
	pgomgr /merge $(PGD)
	pgomgr /summary $(PGD) > $(PGOLOG)
	$(LD) $(PGOLDFLAGS2) $(OBJS) User32.lib

prof-clean:
	del /q *.pgc
	del /q *.pgd
	del    $(PGOLOG)
	del    $(EXE)
