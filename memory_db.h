#ifndef MEMORY_DB_H
#define MEMORY_DB_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <openssl/md5.h>

// 记忆数据结构（对应conversation_mem表）
struct ConversationMem {
    std::string uid;
    std::string user_text;
    std::string robot_text;
    time_t timestamp;
    std::string scene_tag;
    int is_core;
    std::string image_path;
};

// 用户档案结构（对应user_profile表）
struct UserProfile {
    std::string uid;
    std::string face_feature;
    std::string user_type;
    time_t create_time;
};

class MemoryDB {
private:
    sqlite3* db;          // 声明顺序1
    std::string db_path;  // 声明顺序2
    std::string md5(const std::string& input);  // MD5哈希生成

public:
    MemoryDB(const std::string& path);
    ~MemoryDB();
    bool init_db();                  // 初始化数据库（创建表）
    bool insert_memory(const std::string& key, const std::string& value);  // KV插入
    std::string query_memory(const std::string& key);  // KV查询
    bool is_open() const;            // 检查数据库是否打开

    // 核心业务函数
    std::string getUserUID(const std::string& face_feature);
    bool saveConversationMem(const ConversationMem& mem);
    std::vector<ConversationMem> getUserContextMem(const std::string& uid, int top_k);
};

#endif // MEMORY_DB_H