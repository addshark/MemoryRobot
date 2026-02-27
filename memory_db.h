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

// memory_db.h 中补充声明
class MemoryDB {
private:
    sqlite3* db;
    std::string db_path;
    std::string md5(const std::string& input);  // 已有的MD5方法

public:
    MemoryDB(const std::string& path);
    ~MemoryDB();  // 析构函数：关闭数据库
    bool init_db();  // 初始化数据库（创建表）
    bool insert_memory(const std::string& key, const std::string& value);  // 插入数据
    std::string query_memory(const std::string& key);  // 查询数据
    bool is_open() const;  // 检查数据库是否打开
};


#endif // MEMORY_DB_H