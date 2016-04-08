#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>

#include "m_bloomfilter.h"


int main(void) {
  static BaseBloomFilter stBloomFilter = {0};

  // 初始化BloomFilter(容纳100,000元素，假阳概率不超过万分之一)：
  InitBloomFilter(&stBloomFilter, 0, 100000, 0.00001);
  // 重置BloomFilter：
  ResetBloomFilter(&stBloomFilter);
  // 释放BloomFilter:
  FreeBloomFilter(&stBloomFilter);

  // 向BloomFilter中新增一个数值（0-正常，1-加入数值过多）：
  uint32_t dwValue, iRet;
  iRet = BloomFilter_Add(&stBloomFilter, &dwValue, sizeof(uint32_t));

  // 检查数值是否在BloomFilter内（0-存在，1-不存在）：
  iRet = BloomFilter_Check(&stBloomFilter, &dwValue, sizeof(uint32_t));

  // (1.1新增) 将生成好的BloomFilter写入文件:
  iRet = SaveBloomFilterToFile(&stBloomFilter, "dump.bin");
  // (1.1新增) 从文件读取生成好的BloomFilter:
  iRet = LoadBloomFilterFromFile(&stBloomFilter, "dump.bin");

  return 0;
}
