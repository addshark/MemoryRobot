#ifndef MEMORY_DB_H
#define MEMORY_DB_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <openssl/md5.h>  // 生成UID的MD5哈希

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
    sqlite3* db;
    std::string db_path;

    // MD5哈希生成UID（和Python版一致）
    std::string md5(const std::string& input);

public:
    // 构造函数：初始化数据库连接
    MemoryDB(const std::string& path = "/var/lib/robot_memory_cpp.db");
    // 析构函数：关闭数据库连接
    ~MemoryDB();

    // 初始化数据库（创建表）
    bool initDB();

    // 获取/生成用户UID（绑定人脸特征）
    std::string getUserUID(const std::string& face_feature);

    // 保存对话记忆
    bool saveConversationMem(const ConversationMem& mem);

    // 获取用户上下文记忆（用于豆包API上下文）
    std::vector<ConversationMem> getUserContextMem(const std::string& uid, int top_k = 5);
};

#endif // MEMORY_DB_H