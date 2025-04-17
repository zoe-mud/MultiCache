#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <algorithm>

#include "KICachePolicy.h"
#include "KLfuCache.h"
#include "KLruCache.h"
#include "KArcCache/KArcCache.h"

class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

// 辅助函数：打印结果
void printResults(const std::string& testName, int capacity, 
                 const std::vector<int>& get_operations, 
                 const std::vector<int>& hits) {
    std::cout << "=== " << testName << " 结果汇总 ===" << std::endl;
    std::cout << "缓存大小: " << capacity << std::endl;
    
    // 算法名称数组
    std::vector<std::string> names = {"LRU", "LFU", "ARC"};
    
    for (int i = 0; i < names.size(); ++i) {
        double hitRate = 100.0 * hits[i] / get_operations[i];
        std::cout << names[i] << " - 命中率: " << std::fixed << std::setprecision(2) 
                  << hitRate << "% ";
        // 添加具体命中次数和总操作次数
        std::cout << "(" << hits[i] << "/" << get_operations[i] << ")" << std::endl;
    }
    
    std::cout << std::endl;  // 添加空行，使输出更清晰
}

void testHotDataAccess() {
    std::cout << "\n=== 测试场景1：热点数据访问测试 ===" << std::endl;
    
    const int CAPACITY = 20;         // 缓存容量
    const int OPERATIONS = 500000;   // 总操作次数
    const int HOT_KEYS = 20;         // 热点数据数量
    const int COLD_KEYS = 5000;      // 冷数据数量
    
    KamaCache::KLruCache<int, std::string> lru(CAPACITY);
    KamaCache::KLfuCache<int, std::string> lfu(CAPACITY);
    KamaCache::KArcCache<int, std::string> arc(CAPACITY);

    std::random_device rd;
    std::mt19937 gen(rd());
    
    // 基类指针指向派生类对象（多态）
    std::array<KamaCache::KICachePolicy<int, std::string>*, 3> caches = {&lru, &lfu, &arc};
    std::vector<int> hits(3, 0);
    std::vector<int> get_operations(3, 0);
    std::vector<std::string> names = {"LRU", "LFU", "ARC"};

    // 为所有的缓存对象进行相同的操作序列测试
    for (int i = 0; i < caches.size(); ++i) {
        // 先预热缓存，插入一些数据
        for (int key = 0; key < HOT_KEYS; ++key) {
            std::string value = "value" + std::to_string(key);
            caches[i]->put(key, value);
        }
        
        // 交替进行put和get操作，模拟真实场景
        for (int op = 0; op < OPERATIONS; ++op) {
            // 大多数缓存系统中读操作比写操作频繁
            // 所以设置30%概率进行写操作
            bool isPut = (gen() % 100 < 30); 
            int key;
            
            // 70%概率访问热点数据，30%概率访问冷数据
            if (gen() % 100 < 70) {
                key = gen() % HOT_KEYS; // 热点数据
            } else {
                key = HOT_KEYS + (gen() % COLD_KEYS); // 冷数据
            }
            
            if (isPut) {
                // 执行put操作
                std::string value = "value" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            } else {
                // 执行get操作并记录命中情况
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }

    // 打印测试结果
    printResults("热点数据访问测试", CAPACITY, get_operations, hits);
}

void testLoopPattern() {
    std::cout << "\n=== 测试场景2：循环扫描测试 ===" << std::endl;
    
    const int CAPACITY = 40;          // 缓存容量
    const int LOOP_SIZE = 500;        // 循环范围大小
    const int OPERATIONS = 200000;    // 总操作次数
    
    KamaCache::KLruCache<int, std::string> lru(CAPACITY);
    KamaCache::KLfuCache<int, std::string> lfu(CAPACITY);
    KamaCache::KArcCache<int, std::string> arc(CAPACITY);

    std::array<KamaCache::KICachePolicy<int, std::string>*, 3> caches = {&lru, &lfu, &arc};
    std::vector<int> hits(3, 0);
    std::vector<int> get_operations(3, 0);
    std::vector<std::string> names = {"LRU", "LFU", "ARC"};

    std::random_device rd;
    std::mt19937 gen(rd());

    // 为每种缓存算法运行相同的测试
    for (int i = 0; i < caches.size(); ++i) {
        
        // 先预热一部分数据（只加载20%的数据）
        for (int key = 0; key < LOOP_SIZE / 5; ++key) {
            std::string value = "loop" + std::to_string(key);
            caches[i]->put(key, value);
        }
        
        // 设置循环扫描的当前位置
        int current_pos = 0;
        
        // 交替进行读写操作，模拟真实场景
        for (int op = 0; op < OPERATIONS; ++op) {
            // 20%概率是写操作，80%概率是读操作
            bool isPut = (gen() % 100 < 20);
            int key;
            
            // 按照不同模式选择键
            if (op % 100 < 60) {  // 60%顺序扫描
                key = current_pos;
                current_pos = (current_pos + 1) % LOOP_SIZE;
            } else if (op % 100 < 90) {  // 30%随机跳跃
                key = gen() % LOOP_SIZE;
            } else {  // 10%访问范围外数据
                key = LOOP_SIZE + (gen() % LOOP_SIZE);
            }
            
            if (isPut) {
                // 执行put操作，更新数据
                std::string value = "loop" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            } else {
                // 执行get操作并记录命中情况
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result)) {
                    hits[i]++;
                }
            }
        }
    }

    printResults("循环扫描测试", CAPACITY, get_operations, hits);
}

void testWorkloadShift() {
    std::cout << "\n=== 测试场景3：工作负载剧烈变化测试 ===" << std::endl;
    
    const int CAPACITY = 4;            
    const int OPERATIONS = 80000;      
    const int PHASE_LENGTH = OPERATIONS / 5;
    
    KamaCache::KLruCache<int, std::string> lru(CAPACITY);
    KamaCache::KLfuCache<int, std::string> lfu(CAPACITY);
    KamaCache::KArcCache<int, std::string> arc(CAPACITY);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::array<KamaCache::KICachePolicy<int, std::string>*, 3> caches = {&lru, &lfu, &arc};
    std::vector<int> hits(3, 0);
    std::vector<int> get_operations(3, 0);

    // 先填充一些初始数据
    for (int i = 0; i < caches.size(); ++i) {
        for (int key = 0; key < 1000; ++key) {
            std::string value = "init" + std::to_string(key);
            caches[i]->put(key, value);
        }
        
        // 然后进行多阶段测试
        for (int op = 0; op < OPERATIONS; ++op) {
            int key;
            // 根据不同阶段选择不同的访问模式
            if (op < PHASE_LENGTH) {  // 热点访问
                key = gen() % 5;
            } else if (op < PHASE_LENGTH * 2) {  // 大范围随机
                key = gen() % 1000;
            } else if (op < PHASE_LENGTH * 3) {  // 顺序扫描
                key = (op - PHASE_LENGTH * 2) % 100;
            } else if (op < PHASE_LENGTH * 4) {  // 局部性随机
                int locality = (op / 1000) % 10;
                key = locality * 20 + (gen() % 20);
            } else {  // 混合访问
                int r = gen() % 100;
                if (r < 30) {
                    key = gen() % 5;
                } else if (r < 60) {
                    key = 5 + (gen() % 95);
                } else {
                    key = 100 + (gen() % 900);
                }
            }
            
            std::string result;
            get_operations[i]++;
            if (caches[i]->get(key, result)) {
                hits[i]++;
            }
            
            // 随机进行put操作，更新缓存内容
            if (gen() % 100 < 30) {  // 30%概率进行put
                std::string value = "new" + std::to_string(key);
                caches[i]->put(key, value);
            }
        }
    }

    printResults("工作负载剧烈变化测试", CAPACITY, get_operations, hits);
}

int main() {
    testHotDataAccess();
    testLoopPattern();
    testWorkloadShift();
    return 0;
}