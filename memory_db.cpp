#include "memory_db.h"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>

// MD5哈希实现（生成UID）
std::string MemoryDB::md5(const std::string& input) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((const unsigned char*)input.c_str(), input.size(), digest);
    
    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return oss.str().substr(0, 20); // 取前20位，和Python版一致
}

// 构造函数：初始化数据库连接（修复目录创建逻辑）
MemoryDB::MemoryDB(const std::string& path) : db(nullptr), db_path(path) {
    //处理目录创建：兼容相对路径/绝对路径
    std::string dir;
    size_t last_slash = db_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        dir = db_path.substr(0, last_slash);
    } else {
        dir = "."; // 相对路径，使用当前目录
    }

    // 创建目录（递归创建，确保存在）
    struct stat st;
    if (stat(dir.c_str(), &st) != 0) {
        if (mkdir(dir.c_str(), 0777) != 0) {
            std::cerr << "目录创建失败: " << dir << " (" << strerror(errno) << ")" << std::endl;
        } else {
            std::cout << "目录创建成功: " << dir << std::endl;
        }
    }

    // 打开数据库
   int rc = sqlite3_open(":memory:", &db); 
    if (rc != SQLITE_OK) {
        std::cerr << "数据库打开失败：" << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        db = nullptr;
    } else {
        std::cout << "内存数据库打开成功（无需磁盘文件）" << std::endl;
    }
}

// 析构函数：释放数据库资源
MemoryDB::~MemoryDB() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
        std::cout << "数据库连接已关闭" << std::endl;
    }
}

// 初始化数据库（创建表）
bool MemoryDB::init_db() {
    if (!db) {
        std::cerr << "数据库未打开，无法初始化" << std::endl;
        return false;
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

    // 3. 创建KV测试表
    const char* create_kv_sql = R"(
        CREATE TABLE IF NOT EXISTS kv_mem (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            timestamp INTEGER NOT NULL
        );
    )";

    char* err_msg = nullptr;
    int rc;

    // 执行创建用户表
    rc = sqlite3_exec(db, create_user_sql, nullptr, nullptr, &err_msg);
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

    // 执行创建KV表
    rc = sqlite3_exec(db, create_kv_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "创建KV表失败：" << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    std::cout << "数据库表初始化成功" << std::endl;
    return true;
}

// KV插入（测试用）
bool MemoryDB::insert_memory(const std::string& key, const std::string& value) {
    if (!db) return false;

    time_t now = time(nullptr);
    std::string insert_sql = "INSERT OR REPLACE INTO kv_mem (key, value, timestamp) "
                             "VALUES ('" + key + "', '" + value + "', " + std::to_string(now) + ");";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, insert_sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "插入KV数据失败：" << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

// KV查询（测试用）
std::string MemoryDB::query_memory(const std::string& key) {
    if (!db) return "";

    std::string result;
    std::string select_sql = "SELECT value FROM kv_mem WHERE key = '" + key + "';";

    auto callback = [](void* data, int argc, char** argv, char** azColName) -> int {
        if (argc > 0 && argv[0]) {
            *(std::string*)data = argv[0];
        }
        return 0;
    };

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, select_sql.c_str(), callback, &result, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "查询KV数据失败：" << err_msg << std::endl;
        sqlite3_free(err_msg);
        return "";
    }

    return result;
}

// 检查数据库是否打开
bool MemoryDB::is_open() const {
    return db != nullptr;
}

// 获取/生成用户UID
std::string MemoryDB::getUserUID(const std::string& face_feature) {
    if (!db) return "unknown_uid";

    std::string uid = md5(face_feature);
    char* err_msg = nullptr;
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
        std::cout << "新建用户成功，UID: " << uid << std::endl;
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

    char* err_msg = nullptr;
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

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, select_sql.c_str(), callback, &mems, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "查询记忆失败：" << err_msg << std::endl;
        sqlite3_free(err_msg);
        return mems;
    }

    return mems;
}

// 测试主函数
int main() {
    std::string db_path = "./test.db"; 
    MemoryDB db(db_path);
    
    // 检查数据库是否打开
    if (!db.is_open()) {
        std::cerr << "错误：数据库未打开" << std::endl;
        return 1;
    }

    // 初始化数据库
    if (db.init_db()) {
        std::cout << "=== 数据库初始化成功 ===" << std::endl;
        
        // 测试KV插入
        if (db.insert_memory("test_key", "test_value")) {
            std::cout << "KV数据插入成功" << std::endl;
            
            // 测试KV查询
            std::string value = db.query_memory("test_key");
            if (!value.empty()) {
                std::cout << "KV查询结果: " << value << std::endl;
            } else {
                std::cerr << "KV查询失败：未找到数据" << std::endl;
            }
        } else {
            std::cerr << "KV数据插入失败" << std::endl;
        }

        // 测试用户UID生成
        std::string face_feature = "test_face_feature_123";
        std::string uid = db.getUserUID(face_feature);
        std::cout << "用户UID: " << uid << std::endl;

        // 测试对话记忆保存
        ConversationMem mem;
        mem.uid = uid;
        mem.user_text = "你好，机器人";
        mem.robot_text = "你好呀！";
        mem.timestamp = time(nullptr);
        mem.scene_tag = "home";
        mem.is_core = 1;
        mem.image_path = "";
        if (db.saveConversationMem(mem)) {
            std::cout << "对话记忆保存成功" << std::endl;
            
            // 测试对话记忆查询
            auto mems = db.getUserContextMem(uid, 5);
            std::cout << "查询到" << mems.size() << "条记忆：" << std::endl;
            for (const auto& m : mems) {
                std::cout << "用户说：" << m.user_text << " | 机器人回复：" << m.robot_text << std::endl;
            }
        } else {
            std::cerr << "对话记忆保存失败" << std::endl;
        }

    } else {
        std::cerr << "错误：数据库初始化失败" << std::endl;
        return 1;
    }

    return 0;
}