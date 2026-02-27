#include "memory_db.h"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

// MD5哈希实现（生成UID）
std::string MemoryDB::md5(const std::string& input) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((const unsigned char*)input.c_str(), input.size(), digest);
    
    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    // 取前20位，和Python版一致
    return oss.str().substr(0, 20);
}

// 构造函数：打开数据库连接
MemoryDB::MemoryDB(const std::string& path) : db_path(path), db(nullptr) {
    // 确保目录存在
    std::string dir = db_path.substr(0, db_path.find_last_of('/'));
    struct stat st;
    if (stat(dir.c_str(), &st) != 0) {
        mkdir(dir.c_str(), 0777);
    }

    // 打开数据库
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "数据库打开失败：" << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        db = nullptr;
    }
}

// 析构函数：关闭数据库
MemoryDB::~MemoryDB() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

// 初始化数据库（创建表）
bool MemoryDB::initDB() {
    if (!db) {
        // 打开数据库
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::cerr << "打开数据库失败: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            db = nullptr;
            return false;
        }
    }
    // 1. 创建用户档案表
    const char* create_user_sql = R"(
        CREATE TABLE IF NOT EXISTS user_profile (
            uid TEXT PRIMARY KEY,
            face_feature TEXT NOT NULL,
            user_type TEXT NOT NULL,
            create_time INTEGER NOT NULL
        );
    )";

    // 2. 创建对话记忆表
    const char* create_mem_sql = R"(
        CREATE TABLE IF NOT EXISTS conversation_mem (
            mem_id INTEGER PRIMARY KEY AUTOINCREMENT,
            uid TEXT NOT NULL,
            user_text TEXT NOT NULL,
            robot_text TEXT NOT NULL,
            timestamp INTEGER NOT NULL,
            scene_tag TEXT,
            is_core INTEGER DEFAULT 0,
            image_path TEXT,
            FOREIGN KEY (uid) REFERENCES user_profile(uid)
        );
        CREATE INDEX IF NOT EXISTS idx_uid ON conversation_mem(uid);
        CREATE INDEX IF NOT EXISTS idx_timestamp ON conversation_mem(timestamp);
    )";

    char* err_msg;
    // 执行创建用户表
    int rc = sqlite3_exec(db, create_user_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "创建用户表失败：" << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    // 执行创建记忆表
    rc = sqlite3_exec(db, create_mem_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "创建记忆表失败：" << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    return true;
}

// 获取/生成用户UID
std::string MemoryDB::getUserUID(const std::string& face_feature) {
    if (!db) return "unknown_uid";

    std::string uid = md5(face_feature);
    char* err_msg;
    std::string select_sql = "SELECT uid FROM user_profile WHERE uid = '" + uid + "';";

    // 回调函数：检查用户是否存在
    bool user_exists = false;
    auto callback = [](void* data, int argc, char** argv, char** azColName) -> int {
        *(bool*)data = true;
        return 0;
    };

    // 执行查询
    int rc = sqlite3_exec(db, select_sql.c_str(), callback, &user_exists, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "查询用户失败：" << err_msg << std::endl;
        sqlite3_free(err_msg);
        return "unknown_uid";
    }

    // 不存在则新建用户
    if (!user_exists) {
        time_t now = time(nullptr);
        std::string insert_sql = "INSERT INTO user_profile (uid, face_feature, user_type, create_time) "
                                 "VALUES ('" + uid + "', '" + face_feature + "', 'child', " + std::to_string(now) + ");";
        
        rc = sqlite3_exec(db, insert_sql.c_str(), nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::cerr << "插入用户失败：" << err_msg << std::endl;
            sqlite3_free(err_msg);
            return "unknown_uid";
        }
    }

    return uid;
}

// 保存对话记忆
bool MemoryDB::saveConversationMem(const ConversationMem& mem) {
    if (!db) return false;

    std::string insert_sql = "INSERT INTO conversation_mem (uid, user_text, robot_text, timestamp, scene_tag, is_core, image_path) "
                             "VALUES ('" + mem.uid + "', '" + mem.user_text + "', '" + mem.robot_text + "', " + 
                             std::to_string(mem.timestamp) + ", '" + mem.scene_tag + "', " + std::to_string(mem.is_core) + 
                             ", '" + mem.image_path + "');";

    char* err_msg;
    int rc = sqlite3_exec(db, insert_sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "保存记忆失败：" << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    return true;
}

// 获取用户上下文记忆
std::vector<ConversationMem> MemoryDB::getUserContextMem(const std::string& uid, int top_k) {
    std::vector<ConversationMem> mems;
    if (!db) return mems;

    std::string select_sql = "SELECT uid, user_text, robot_text, timestamp, scene_tag, is_core, image_path "
                             "FROM conversation_mem WHERE uid = '" + uid + "' "
                             "ORDER BY timestamp DESC LIMIT " + std::to_string(top_k) + ";";

    // 回调函数：解析查询结果
    auto callback = [](void* data, int argc, char** argv, char** azColName) -> int {
        std::vector<ConversationMem>* mems = (std::vector<ConversationMem>*)data;
        ConversationMem mem;
        mem.uid = argv[0] ? argv[0] : "";
        mem.user_text = argv[1] ? argv[1] : "";
        mem.robot_text = argv[2] ? argv[2] : "";
        mem.timestamp = argv[3] ? atol(argv[3]) : 0;
        mem.scene_tag = argv[4] ? argv[4] : "";
        mem.is_core = argv[5] ? atoi(argv[5]) : 0;
        mem.image_path = argv[6] ? argv[6] : "";
        mems->push_back(mem);
        return 0;
    };

    char* err_msg;
    int rc = sqlite3_exec(db, select_sql.c_str(), callback, &mems, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "查询记忆失败：" << err_msg << std::endl;
        sqlite3_free(err_msg);
        return mems;
    }

    return mems;
}
// 在memory_db.cpp末尾添加
#include <iostream>
int main() {
    // 实例化MemoryDB对象
    MemoryDB db("test.db");
    
    // 初始化数据库
    if (db.init_db()) {
        // 插入数据
        if (db.insert_memory("test_key", "test_value")) {
            // 查询数据
            std::string value = db.query_memory("test_key");
            if (!value.empty()) {
                std::cout << "查询结果: " << value << std::endl;
            } else {
                std::cerr << "查询不到数据" << std::endl;
            }
        } else {
            std::cerr << "插入数据失败" << std::endl;
        }
    } else {
        std::cerr << "初始化数据库失败" << std::endl;
        return 1;
    }

    return 0;
}