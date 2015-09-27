// ConsoleApplication4.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"

short pc_on_sq[nsquare][pos_n];
short kkp[nsquare][nsquare][kkp_end];
short pc_on_sq2[nsquare][pos_n * 2];
short kk[nsquare][nsquare];

FILE * file_open(const char *str_file, const char *str_mode);
int file_close(FILE *pf);

int main(void)
{
    printf("ミッションスタート\n");
    FILE *pf;
    FILE *pf2;
    size_t size;
    int iret;

    pf = file_open("fv38.bin", "rb");
    if (pf == NULL) { return -2; }
    printf("fv38.binオープン成功\n");

    //KPP
    size = nsquare * pos_n;
    if (fread(pc_on_sq, sizeof(short), size, pf) != size)
    {
        //str_error = str_io_error;
        return -2;
    }
    printf("KPP読み込み成功\n");
    //KKP
    size = nsquare * nsquare * kkp_end;
    if (fread(kkp, sizeof(short), size, pf) != size)
    {
        //str_error = str_io_error;
        return -2;
    }
    printf("KKP読み込み成功\n");
	//KK
	size = nsquare * nsquare;
	if (fread(kk, sizeof(short), size, pf) != size)
	{
		//str_error = str_io_error;
		return -2;
	}
	printf("KK読み込み成功\n");

    //書き込みファイルオープン
    pf2 = file_open("eval38.bin", "wb");
    if (pf2 == NULL) { return -2; }
    printf("書き込みファイルeval38.bin作成成功\n");

    //KPP計算
    int i, j, k;
    for (k = 0; k < 81; k++)
    {
        printf("k=%d・・・",k);
        for (i = 0; i < fe_end; i++)
        {
            for (j = 0; j < fe_end; j++)
            {
                if (j <= i)
                {
                    PcPcOnSq2(k, i, j) = PcPcOnSq(k, i, j);
                }
                else {
                    PcPcOnSq2(k, i, j) = PcPcOnSq(k, j, i);
                }
            };
        };
    };
    printf("KPP代入成功\n");

    //KPP書き込み
    size = nsquare * pos_n * 2;
    if (fwrite(pc_on_sq2, sizeof(short), size, pf2) != size)
    {
        //str_error = str_io_error;
        return -2;
    }
    printf("KPP書き込み成功\n");
    //KKP書き込み
    size = nsquare * nsquare * kkp_end;
    if (fwrite(kkp, sizeof(short), size, pf2) != size)
    {
        //str_error = str_io_error;
        return -2;
    }
    printf("KKP書き込み成功\n");
	//KK書き込み
    size = nsquare * nsquare;
	if (fwrite(kk, sizeof(short), size, pf2) != size)
	{
		//str_error = str_io_error;
		return -2;
	}
	printf("KK書き込み成功\n");

    iret = file_close(pf);
    if (iret < 0) { return iret; }
    printf("ファイルクローズ成功１\n");
    iret = file_close(pf2);
    if (iret < 0) { return iret; }
    printf("ファイルクローズ成功２\n");

    printf("終わり\n");
    return 0;
}

FILE * file_open(const char *str_file, const char *str_mode)
{
    FILE *pf;

    pf = fopen(str_file, str_mode);
    if (pf == NULL)
    {
        /*snprintf(str_message, SIZE_MESSAGE,
        "%s, %s", str_fopen_error, str_file);
        str_error = str_message;*/
        printf("ファイルオープン失敗");
        return NULL;
    }

    return pf;
}

int file_close(FILE *pf)
{
    if (pf == NULL) { return 1; }
    if (ferror(pf))
    {
        fclose(pf);
        //str_error = str_io_error;
        printf("ファイルクローズ失敗");
        return -2;
    }
    if (fclose(pf))
    {
        //str_error = str_io_error;
        printf("ファイルクローズ失敗");
        return -2;
    }

    return 1;
}