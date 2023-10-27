#include <iostream>
#include <fstream>
#include <cstring>
#define FILE_CIPHER "cipher.txt"
#define FILE_PTS "pts.txt"
#define PTS_NUMBER 20000
#define TRACE_NUMBER 100
int BLOCK_NUM = 0;                    // 选取的块号
double pts[PTS_NUMBER][TRACE_NUMBER]; // 从文件中读取20000行/每行100个功耗跟踪记录
unsigned char cipher[PTS_NUMBER][16]; // 从文件中读取20000条/每条16块密文
int blockBit[PTS_NUMBER][256][8];     // 20000条密文，每条密文的选取的块经过256种猜测的轮密钥块解密后的8bit情况
char buffer[10240];
double sum0[256][TRACE_NUMBER][8], sum1[256][TRACE_NUMBER][8]; // 保存0/1功耗和
int count0[256][8], count1[256][8];                            // 记录0/1分别的个数
double result[256][TRACE_NUMBER];                              // 保存最后的计算结果，256*100

// 每个块经过逆行移位后的位置
static const int invShiftRowsPos[16] = {0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12, 1, 6, 11};

// 逆sbox
static const unsigned char invSBox[256] =
    {
        0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
        0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
        0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
        0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
        0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
        0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
        0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
        0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
        0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
        0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
        0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
        0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
        0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
        0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
        0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
        0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d};

static void invShiftRows(unsigned char *p)
{
    unsigned char row;
    unsigned char col;
    unsigned char temp;
    unsigned char rowData[4];
    for (row = 1; row < 4; row++)
    {
        temp = 4 - row;
        for (col = 0; col < 4; col++)
        {
            rowData[col] = p[row + 4 * col];
        }
        for (col = 0; col < 4; col++)
        {
            p[row + 4 * col] = rowData[(col + temp) % 4];
        }
    }
}

void load_pts()
{
    FILE *fp;
    int i;

    fp = fopen(FILE_PTS, "r");
    if (fp == NULL)
    {
        std::cout << "Cannot open the power trace file.\n";
        exit(0);
    }

    for (i = 0; i < PTS_NUMBER; i++)
    {
        int j;
        char *p;
        double *pf = pts[i];

        if (!fgets(buffer, 10240, fp))
        {
            std::cout << "Error: reading pt " << i << std::endl;
            exit(0);
        }
        p = buffer;
        for (j = 0; j < TRACE_NUMBER; j++)
        {
            int iv;
            char *curp = p;

            while (*p && *p != ',')
                p++;
            *p++ = 0;

            iv = atoi(curp);
            *pf++ = (double)iv;
        }
    }
    fclose(fp);
}
void load_cipher()
{
    int i;
    FILE *fp;

    fp = fopen(FILE_CIPHER, "r");

    if (fp == NULL)
    {
        std::cout << "Cannot open cipher file." << std::endl;
        exit(0);
    }

    for (i = 0; i < PTS_NUMBER; i++)
    {
        int j;
        char *p;

        if (!fgets(buffer, 100, fp))
        {
            std::cout << "Error: reading cipher " << i << std::endl;
            exit(0);
        }
        p = buffer;
        for (j = 0; j < 16; j++)
        {
            int v;
            int c;

            c = *p++;
            v = (c >= 'a') ? (c - 'a' + 10) : (c - '0');

            c = *p++;
            v = (v << 4) | ((c >= 'a') ? (c - 'a' + 10) : (c - '0'));

            cipher[i][j] = v;
        }
    }
    fclose(fp);
}

// 调用20000次，处理200000行密文
// 每次调用只处理选定的密文块，轮密钥的每个块有256种猜测，遍历每种猜测并进行aes第10轮的逆变换，保存200000条密文选定块的256种轮密钥猜测逆变换后每一位
// c指向当前密文，num为当前密文的序号
void generateBit(unsigned char *cipher, unsigned int num)
{
    for (int i = 0; i < 256; i++) // 对轮密钥的每个block有256种猜测
    {

        unsigned char cipherText[16] = {0x00};          // 初始化密文副本的当前块
        cipherText[BLOCK_NUM] = cipher[BLOCK_NUM] ^ i;  // 对选定密文块进行轮密钥加
        invShiftRows(cipherText);                       // 对密文进行逆行移位
        int index = invShiftRowsPos[BLOCK_NUM];         // 得到选定密文块逆行移位后的位置
        cipherText[index] = invSBox[cipherText[index]]; // 逆字节代换
        for (int bit = 0; bit < 8; bit++)               // 保存选定块的每种轮密钥猜测的每一位
        {
            blockBit[num][i][bit] = ((cipherText[index] ^ cipher[index]) >> (7 - bit)) & 0x01;
        }
    }
}

// 根据generateBit()的结果对功耗进行分类、计算
void DPA()
{
    // 初始化
    for (int i = 0; i < 256; i++)
    {
        for (int j = 0; j < TRACE_NUMBER; j++)
        {
            result[i][j] = 0.0;
            for (int k = 0; k < 8; k++)
            {
                sum0[i][j][k] = sum1[i][j][k] = 0.0;
                count0[i][k] = count1[i][k] = 0;
            }
        }
    }

    for (int i = 0; i < PTS_NUMBER; i++)
    {
        for (int j = 0; j < 256; j++)
        {
            for (int k = 0; k < 8; k++)
            {
                if (blockBit[i][j][k] == 0)
                {
                    for (int n = 0; n < TRACE_NUMBER; n++)
                    {
                        sum0[j][n][k] += pts[i][n];
                        count0[j][k]++;
                    }
                }
                else
                {
                    for (int n = 0; n < TRACE_NUMBER; n++)
                    {
                        sum1[j][n][k] += pts[i][n];
                        count1[j][k]++;
                    }
                }
            }
        }
    }

    for (int i = 0; i < 256; i++)
    {
        for (int j = 0; j < TRACE_NUMBER; j++)
        {
            for (int k = 0; k < 8; k++)
            {
                sum0[i][j][k] /= count0[i][k];
                sum1[i][j][k] /= count1[i][k];
                result[i][j] += sum0[i][j][k] - sum1[i][j][k];
            }
            result[i][j] = abs(result[i][j]);
        }
    }

    std::ofstream file;
    std::string filename = "result" + std::to_string(BLOCK_NUM) + ".txt";
    file.open(filename);
    for (int i = 0; i < 256; i++)
    {
        std::string buf = "";
        for (int j = 0; j < TRACE_NUMBER; j++)
        {
            buf = buf + std::to_string(result[i][j]) + ",";
        }
        file << buf << std::endl;
    }
    std::cout << "result" << BLOCK_NUM << " is ok." << std::endl;
}

int main()
{
    load_cipher();
    load_pts();
    for (BLOCK_NUM = 0; BLOCK_NUM < 16; BLOCK_NUM++)
    {
        for (int i = 0; i < PTS_NUMBER; i++)
        {
            generateBit(cipher[i], i);
        }
        DPA();
    }
    return 0;
}