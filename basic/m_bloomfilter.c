#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "m_bloomfilter.h"

// 计算BloomFilter的参数m,k
static void _CalcBloomFilterParam(uint32_t n, double p, uint32_t *pm, uint32_t *pk)
{
    /**
     *  n - Number of items in the filter
     *  p - Probability of false positives, float between 0 and 1 or a number indicating 1-in-p
     *  m - Number of bits in the filter
     *  k - Number of hash functions
     *
     *  f = ln(2) × ln(1/2) × m / n = (0.6185) ^ (m/n)
     *  m = -1 * ln(p) × n / 0.6185
     *  k = ln(2) × m / n = 0.6931 * m / n
    **/

    uint32_t m, k;

    // 计算指定假阳概率下需要的比特数
    m = (uint32_t) ceil(-1 * log(p) * n / 0.6185);
    m = (m - m % 64) + 64;                  // 8字节对齐

    // 计算哈希函数个数
    k = (uint32_t) (0.6931 * m / n);
    k++;

    *pm = m;
    *pk = k;
    return;
}


// 根据目标精度和数据个数，初始化BloomFilter结构
int InitBloomFilter(BaseBloomFilter *pstBloomfilter, uint32_t dwSeed, uint32_t dwMaxItems, double dProbFalse)
{
    if (pstBloomfilter == NULL)
        return -1;
    if ((dProbFalse <= 0) || (dProbFalse >= 1))
        return -2;

    // 先检查是否重复Init，释放内存
    if (pstBloomfilter->pstFilter != NULL)
        free(pstBloomfilter->pstFilter);
    if (pstBloomfilter->pdwHashPos != NULL)
        free(pstBloomfilter->pdwHashPos);

    memset(pstBloomfilter, 0, sizeof(BaseBloomFilter));

    // 初始化内存结构，并计算BloomFilter需要的空间
    pstBloomfilter->dwMaxItems = dwMaxItems;
    pstBloomfilter->dProbFalse = dProbFalse;
    pstBloomfilter->dwSeed = dwSeed;

    // 计算 m, k
    _CalcBloomFilterParam(pstBloomfilter->dwMaxItems, pstBloomfilter->dProbFalse,
                    &pstBloomfilter->dwFilterBits, &pstBloomfilter->dwHashFuncs);

    // 分配BloomFilter的存储空间
    pstBloomfilter->dwFilterSize = pstBloomfilter->dwFilterBits / BYTE_BITS;
    pstBloomfilter->pstFilter = (unsigned char *) malloc(pstBloomfilter->dwFilterSize);
    if (NULL == pstBloomfilter->pstFilter)
        return -100;

    // 哈希结果数组，每个哈希函数一个
    pstBloomfilter->pdwHashPos = (uint32_t*) malloc(pstBloomfilter->dwHashFuncs * sizeof(uint32_t));
    if (NULL == pstBloomfilter->pdwHashPos)
        return -200;

    printf(">>> Init BloomFilter(n=%u, p=%f, m=%u, k=%d), malloc() size=%.2fMB\n",
            pstBloomfilter->dwMaxItems, pstBloomfilter->dProbFalse, pstBloomfilter->dwFilterBits,
            pstBloomfilter->dwHashFuncs, (double)pstBloomfilter->dwFilterSize/1024/1024);

    // 初始化BloomFilter的内存
    memset(pstBloomfilter->pstFilter, 0, pstBloomfilter->dwFilterSize);
    pstBloomfilter->cInitFlag = 1;
    return 0;
}

// 释放BloomFilter
int FreeBloomFilter(BaseBloomFilter *pstBloomfilter)
{
    if (pstBloomfilter == NULL)
        return -1;

    pstBloomfilter->cInitFlag = 0;
    pstBloomfilter->dwCount = 0;

    free(pstBloomfilter->pstFilter);
    pstBloomfilter->pstFilter = NULL;
    free(pstBloomfilter->pdwHashPos);
    pstBloomfilter->pdwHashPos = NULL;
    return 0;
}

// 重置BloomFilter
// 注意: Reset()函数不会立即初始化stFilter，而是当一次Add()时去memset
int ResetBloomFilter(BaseBloomFilter *pstBloomfilter)
{
    if (pstBloomfilter == NULL)
        return -1;

    pstBloomfilter->cInitFlag = 0;
    pstBloomfilter->dwCount = 0;
    return 0;
}

// 和ResetBloomFilter不同，调用后立即memset内存
int RealResetBloomFilter(BaseBloomFilter *pstBloomfilter)
{
    if (pstBloomfilter == NULL)
        return -1;

    memset(pstBloomfilter->pstFilter, 0, pstBloomfilter->dwFilterSize);
    pstBloomfilter->cInitFlag = 1;
    pstBloomfilter->dwCount = 0;
    return 0;
}

/* 文件相关封装 */
// 将生成好的BloomFilter写入文件
int SaveBloomFilterToFile(BaseBloomFilter *pstBloomfilter, char *szFileName)
{
    if ((pstBloomfilter == NULL) || (szFileName == NULL))
        return -1;

    int iRet;
    FILE *pFile;
    static BloomFileHead stFileHeader = {0};

    pFile = fopen(szFileName, "wb");
    if (pFile == NULL)
    {
        perror("fopen");
        return -11;
    }

    // 先写入文件头
    stFileHeader.dwMagicCode = __MGAIC_CODE__;
    stFileHeader.dwSeed = pstBloomfilter->dwSeed;
    stFileHeader.dwCount = pstBloomfilter->dwCount;
    stFileHeader.dwMaxItems = pstBloomfilter->dwMaxItems;
    stFileHeader.dProbFalse = pstBloomfilter->dProbFalse;
    stFileHeader.dwFilterBits = pstBloomfilter->dwFilterBits;
    stFileHeader.dwHashFuncs = pstBloomfilter->dwHashFuncs;
    stFileHeader.dwFilterSize = pstBloomfilter->dwFilterSize;

    iRet = fwrite((const void*)&stFileHeader, sizeof(stFileHeader), 1, pFile);
    if (iRet != 1)
    {
        perror("fwrite(head)");
        return -21;
    }

    // 接着写入BloomFilter的内容
    iRet = fwrite(pstBloomfilter->pstFilter, 1, pstBloomfilter->dwFilterSize, pFile);
    if ((uint32_t)iRet != pstBloomfilter->dwFilterSize)
    {
        perror("fwrite(data)");
        return -31;
    }

    fclose(pFile);
    return 0;
}

// 从文件读取生成好的BloomFilter
int LoadBloomFilterFromFile(BaseBloomFilter *pstBloomfilter, char *szFileName)
{
    if ((pstBloomfilter == NULL) || (szFileName == NULL))
        return -1;

    int iRet;
    FILE *pFile;
    static BloomFileHead stFileHeader = {0};

    if (pstBloomfilter->pstFilter != NULL)
        free(pstBloomfilter->pstFilter);
    if (pstBloomfilter->pdwHashPos != NULL)
        free(pstBloomfilter->pdwHashPos);

    //
    pFile = fopen(szFileName, "rb");
    if (pFile == NULL)
    {
        perror("fopen");
        return -11;
    }

    // 读取并检查文件头
    iRet = fread((void*)&stFileHeader, sizeof(stFileHeader), 1, pFile);
    if (iRet != 1)
    {
        perror("fread(head)");
        return -21;
    }

    if ((stFileHeader.dwMagicCode != __MGAIC_CODE__)
        || (stFileHeader.dwFilterBits != stFileHeader.dwFilterSize*BYTE_BITS))
        return -50;

    // 初始化传入的 BaseBloomFilter 结构
    pstBloomfilter->dwMaxItems = stFileHeader.dwMaxItems;
    pstBloomfilter->dProbFalse = stFileHeader.dProbFalse;
    pstBloomfilter->dwFilterBits = stFileHeader.dwFilterBits;
    pstBloomfilter->dwHashFuncs = stFileHeader.dwHashFuncs;
    pstBloomfilter->dwSeed = stFileHeader.dwSeed;
    pstBloomfilter->dwCount = stFileHeader.dwCount;
    pstBloomfilter->dwFilterSize = stFileHeader.dwFilterSize;

    pstBloomfilter->pstFilter = (unsigned char *) malloc(pstBloomfilter->dwFilterSize);
    if (NULL == pstBloomfilter->pstFilter)
        return -100;
    pstBloomfilter->pdwHashPos = (uint32_t*) malloc(pstBloomfilter->dwHashFuncs * sizeof(uint32_t));
    if (NULL == pstBloomfilter->pdwHashPos)
        return -200;


    // 将后面的Data部分读入 pstFilter
    iRet = fread((void*)(pstBloomfilter->pstFilter), 1, pstBloomfilter->dwFilterSize, pFile);
    if ((uint32_t)iRet != pstBloomfilter->dwFilterSize)
    {
        perror("fread(data)");
        return -31;
    }
    pstBloomfilter->cInitFlag = 1;

    printf(">>> Load BloomFilter(n=%u, p=%f, m=%u, k=%d), malloc() size=%.2fMB\n",
        pstBloomfilter->dwMaxItems, pstBloomfilter->dProbFalse, pstBloomfilter->dwFilterBits,
        pstBloomfilter->dwHashFuncs, (double)pstBloomfilter->dwFilterSize/1024/1024);

    fclose(pFile);
    return 0;
}
