 // server.cpp
// Minimal C++ server using cpp-httplib and sqlite3
#include <httplib.h>       // httplib.h header (cpp-httplib)
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp> // optional - single header JSON (or build JSON manually)
#include <curl/curl.h> // for Bedrock call

using json = nlohmann::json;

static int callback(void *data, int argc, char **argv, char **azColName) {
    json *arr = (json*)data;
    json row;
    for (int i = 0; i < argc; i++) {
        row[azColName[i]] = argv[i] ? argv[i] : nullptr;
    }
    arr->push_back(row);
    return 0;
}

void ensure_db(sqlite3* db) {
    const char* sql = R"(CREATE TABLE IF NOT EXISTS feedback (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user TEXT,
        text TEXT NOT NULL,
        category TEXT,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );)";
    char *err = 0;
    sqlite3_exec(db, sql, 0, 0, &err);
    if (err) { std::cerr << "DB create error: " << err << std::endl; sqlite3_free(err); }
}

size_t curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size*nmemb);
    return size*nmemb;
}

std::string call_bedrock(const std::string &prompt) {
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    std::string summary;

    try {
        Aws::Client::ClientConfiguration config;
        config.region = std::getenv("AWS_REGION") ? std::getenv("AWS_REGION") : "us-east-1";

        Aws::BedrockRuntime::BedrockRuntimeClient client(config);
        Aws::BedrockRuntime::Model::InvokeModelRequest request;

        // Choose model — e.g., Anthropic Claude
        request.SetModelId("anthropic.claude-3-sonnet-20240229-v1:0");

        Aws::Utils::Json::JsonValue payload;
        payload.WithString("prompt", "Summarize the following feedbacks into main themes:\n" + prompt);
        payload.WithInteger("max_tokens_to_sample", 300);

        request.SetBody(Aws::MakeShared<Aws::StringStream>("", payload.View().WriteCompact()));
        request.SetContentType("application/json");

        auto outcome = client.InvokeModel(request);

        if (outcome.IsSuccess()) {
            auto &result = outcome.GetResult();
            auto body = result.GetBody().rdbuf();
            std::string resp((std::istreambuf_iterator<char>(body)), std::istreambuf_iterator<char>());
            summary = resp;
        } else {
            summary = "Error from Bedrock: " + outcome.GetError().GetMessage();
        }
    } catch (const std::exception &e) {
        summary = std::string("Exception: ") + e.what();
    }

    Aws::ShutdownAPI(options);
    return summary;
}


int main() {
    sqlite3 *db;
    sqlite3_open("feedback.db", &db);
    ensure_db(db);

    httplib::Server svr;

    // static file serving for frontend (index.html, app.js, styles.css) - served by nginx in production,
    // but for easy testing we can serve from backend folder:
    svr.set_mount_point("/", "./frontend"); // optional for local dev

    svr.Post("/api/feedback", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string user = j.value("user", "");
            std::string text = j.value("text", "");
            std::string category = j.value("category", "other");
            if (text.empty()) {
                res.status = 400;
                res.set_content(R"({"error":"text required"})", "application/json");
                return;
            }
            sqlite3_stmt *stmt;
            const char* insert_sql = "INSERT INTO feedback (user, text, category) VALUES (?, ?, ?);";
            sqlite3_prepare_v2(db, insert_sql, -1, &stmt, 0);
            sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, text.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, category.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            res.set_content(R"({"status":"ok"})", "application/json");
        } catch(...) {
            res.status = 500;
            res.set_content(R"({"error":"invalid input"})", "application/json");
        }
    });

    svr.Get("/api/feedbacks", [&](const httplib::Request& req, httplib::Response& res) {
        json arr = json::array();
        char *err = nullptr;
        const char* q = "SELECT id, user, text, category, created_at FROM feedback ORDER BY created_at DESC LIMIT 100;";
        sqlite3_exec(db, q, callback, &arr, &err);
        if (err) { std::string e(err); sqlite3_free(err); res.status=500; res.set_content(R"({"error":"db"})", "application/json"); return; }
        res.set_content(arr.dump(), "application/json");
    });

    svr.Get("/api/summary", [&](const httplib::Request& req, httplib::Response& res) {
        // gather feedbacks to form a prompt
        json arr = json::array();
        sqlite3_exec(db, "SELECT text FROM feedback ORDER BY created_at DESC LIMIT 50;", callback, &arr, nullptr);
        std::ostringstream prompt;
        prompt << "Summarize the following user feedback into top themes and improvement suggestions:\n\n";
        for (auto &it : arr) prompt << "- " << it["text"].get<std::string>() << "\n";

        std::string ai_resp = call_bedrock(prompt.str());
        json out;
        out["summary"] = ai_resp;
        res.set_content(out.dump(), "application/json");
    });

    std::cout << "Server running on http://0.0.0.0:8080\n";
    svr.listen("0.0.0.0", 8080);

    sqlite3_close(db);
    return 0;
}

