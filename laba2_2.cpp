#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;

std::vector<std::string> order;
std::mutex orderMtx;

double ms(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

std::string now() {
    std::time_t t = std::time(0);
    char b[32];
    std::strftime(b, 32, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return b;
}

void finish(const std::string& url, const std::string& status) {
    std::lock_guard<std::mutex> lock(orderMtx);
    order.push_back("завершён " + url + " [" + status + "]");
}

struct Url {
    std::string host, port, path, query;
};

bool parseUrl(const std::string& text, Url& u) {
    if (text.compare(0, 7, "http://") != 0) return false;
    std::string s = text.substr(7);
    size_t slash = s.find('/');
    std::string hp = slash == std::string::npos ? s : s.substr(0, slash);
    std::string rest = slash == std::string::npos ? "/" : s.substr(slash);

    size_t colon = hp.find(':');
    u.host = colon == std::string::npos ? hp : hp.substr(0, colon);
    u.port = colon == std::string::npos ? "80" : hp.substr(colon + 1);
    if (u.host.empty() || u.port.empty()) return false;
    for (char& c : u.host) c = (char)tolower(c);
    for (char c : u.port) if (c < '0' || c > '9') return false;

    size_t hash = rest.find('#');
    if (hash != std::string::npos) rest = rest.substr(0, hash);
    size_t q = rest.find('?');
    u.path = q == std::string::npos ? rest : rest.substr(0, q);
    u.query = q == std::string::npos ? "" : rest.substr(q);
    if (u.path.empty()) u.path = "/";
    return true;
}

std::string cleanPath(const std::string& path) {
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i <= path.size(); i++) {
        if (i < path.size() && path[i] != '/') {
            cur += path[i];
            continue;
        }
        if (cur == "..") {
            if (!parts.empty()) parts.pop_back();
        } else if (!cur.empty() && cur != ".") {
            parts.push_back(cur);
        }
        cur.clear();
    }

    std::string out = "/";
    for (size_t i = 0; i < parts.size(); i++) out += parts[i] + (i + 1 < parts.size() ? "/" : "");
    if (!path.empty() && path.back() == '/' && out.back() != '/') out += "/";
    return out;
}

std::string buildUrl(const Url& u) {
    return "http://" + u.host + (u.port == "80" ? "" : ":" + u.port) + u.path + u.query;
}

std::string normalizeLink(const Url& base, const std::string& link) {
    std::string s;
    for (char c : link) if (!isspace((unsigned char)c)) s += c;
    size_t hash = s.find('#');
    if (hash != std::string::npos) s = s.substr(0, hash);
    if (s.empty() || s.compare(0, 7, "mailto:") == 0 || s.compare(0, 11, "javascript:") == 0) return "";

    Url u = base;
    if (s.compare(0, 7, "http://") == 0) {
        if (!parseUrl(s, u)) return "";
    } else if (s.compare(0, 2, "//") == 0) {
        if (!parseUrl("http:" + s, u)) return "";
    } else if (s.find("://") != std::string::npos) {
        return "";
    } else {
        size_t q = s.find('?');
        std::string path = q == std::string::npos ? s : s.substr(0, q);
        u.query = q == std::string::npos ? "" : s.substr(q);
        if (path.empty()) path = base.path;
        else if (path[0] != '/') path = base.path.substr(0, base.path.find_last_of('/') + 1) + path;
        u.path = path;
    }

    if (u.host != base.host) return "";
    u.path = cleanPath(u.path);
    return buildUrl(u);
}

std::vector<std::string> extractLinks(const std::string& html) {
    std::string low = html;
    for (char& c : low) c = (char)tolower(c);
    std::vector<std::string> links;
    size_t pos = 0;

    while ((pos = low.find("<a", pos)) != std::string::npos) {
        size_t end = low.find('>', pos);
        if (end == std::string::npos) break;
        size_t h = low.find("href", pos);
        size_t eq = h == std::string::npos ? std::string::npos : low.find('=', h);

        if (h != std::string::npos && h < end && eq != std::string::npos && eq < end) {
            size_t i = eq + 1;
            while (i < end && isspace((unsigned char)low[i])) i++;
            char quote = (i < end && (low[i] == '"' || low[i] == '\'')) ? low[i] : ' ';
            if (quote != ' ') i++;
            size_t from = i;
            while (i < end && low[i] != quote && !(quote == ' ' && isspace((unsigned char)low[i]))) i++;
            links.push_back(html.substr(from, i - from));
        }
        pos = end + 1;
    }
    return links;
}

struct Page {
    std::string url, status = "не загружена";
    int depth = 0;
    std::vector<std::string> links;
};

struct Graph {
    std::vector<Page> pages;
    std::map<std::string, int> index;

    int add(const std::string& url, int depth) {
        if (index.count(url)) return index[url];
        Page p;
        p.url = url;
        p.depth = depth;
        pages.push_back(p);
        index[url] = (int)pages.size() - 1;
        return index[url];
    }

    void links(int page, const std::string& html, int maxDepth, int maxPages, std::vector<std::string>& next) {
        Url base;
        parseUrl(pages[page].url, base);
        for (const std::string& raw : extractLinks(html)) {
            std::string url = normalizeLink(base, raw);
            if (url.empty()) continue;
            if (std::find(pages[page].links.begin(), pages[page].links.end(), url) == pages[page].links.end()) {
                pages[page].links.push_back(url);
            }
            if (index.count(url) || (int)pages.size() >= maxPages || pages[page].depth >= maxDepth) continue;
            next.push_back(url);
            add(url, pages[page].depth + 1);
        }
    }
};

struct Result {
    bool ok = false;
    std::string status, html;
};

void makeRequest(const Url& u, http::request<http::string_body>& req) {
    req.method(http::verb::get);
    req.target(u.path + u.query);
    req.version(11);
    req.set(http::field::host, u.host);
    req.set(http::field::connection, "close");
}

void fetchSync(const std::string& url, int timeout, Result& r) {
    try {
        Url u;
        if (!parseUrl(url, u)) throw std::runtime_error("плохой url");
        asio::io_context io;
        asio::ip::tcp::resolver resolver(io);
        beast::tcp_stream stream(io);

        stream.expires_after(std::chrono::seconds(timeout));
        stream.connect(resolver.resolve(u.host, u.port));
        http::request<http::string_body> req;
        makeRequest(u, req);
        stream.expires_after(std::chrono::seconds(timeout));
        http::write(stream, req);

        beast::flat_buffer buf;
        http::response<http::string_body> res;
        stream.expires_after(std::chrono::seconds(timeout));
        http::read(stream, buf, res);
        r.status = std::to_string(res.result_int());
        r.html = res.body();
        r.ok = res.result_int() == 200;
    } catch (const std::exception& e) {
        r.status = e.what();
    }
    finish(url, r.status);
}

asio::awaitable<void> fetchAsync(asio::io_context* io, std::string url, int timeout, Result* r) {
    try {
        Url u;
        if (!parseUrl(url, u)) throw std::runtime_error("плохой url");
        asio::ip::tcp::resolver resolver(io->get_executor());
        beast::tcp_stream stream(io->get_executor());

        stream.expires_after(std::chrono::seconds(timeout));
        auto points = co_await resolver.async_resolve(u.host, u.port, asio::use_awaitable);
        stream.expires_after(std::chrono::seconds(timeout));
        co_await stream.async_connect(points, asio::use_awaitable);

        http::request<http::string_body> req;
        makeRequest(u, req);
        stream.expires_after(std::chrono::seconds(timeout));
        co_await http::async_write(stream, req, asio::use_awaitable);

        beast::flat_buffer buf;
        http::response<http::string_body> res;
        stream.expires_after(std::chrono::seconds(timeout));
        co_await http::async_read(stream, buf, res, asio::use_awaitable);
        r->status = std::to_string(res.result_int());
        r->html = res.body();
        r->ok = res.result_int() == 200;
    } catch (const std::exception& e) {
        r->status = e.what();
    }
    finish(url, r->status);
}

void crawl(const std::string& start, int maxDepth, int maxPages, int conc, int timeout, bool async, Graph& g) {
    Url base;
    if (!parseUrl(start, base)) return;
    base.path = cleanPath(base.path);

    std::vector<std::string> level;
    level.push_back(buildUrl(base));
    g.add(level[0], 0);

    for (int depth = 0; depth <= maxDepth && !level.empty(); depth++) {
        std::vector<std::string> next;
        for (size_t i = 0; i < level.size(); i += conc) {
            size_t n = std::min((size_t)conc, level.size() - i);
            std::vector<Result> results(n);

            if (async) {
                asio::io_context io;
                for (size_t k = 0; k < n; k++) {
                    asio::co_spawn(io, fetchAsync(&io, level[i + k], timeout, &results[k]), asio::detached);
                }
                io.run();
            } else {
                for (size_t k = 0; k < n; k++) fetchSync(level[i + k], timeout, results[k]);
            }

            for (size_t k = 0; k < n; k++) {
                int page = g.index[level[i + k]];
                g.pages[page].status = results[k].status;
                if (results[k].ok) g.links(page, results[k].html, maxDepth, maxPages, next);
            }
        }
        level = next;
    }
}

bool saveGraph(const std::string& path, const Graph& g) {
    std::ofstream f(path);
    if (!f) return false;
    for (const Page& p : g.pages) f << p.url << " depth=" << p.depth << " status=" << p.status << "\n";
    f << "рёбра:\n";
    for (const Page& p : g.pages) {
        for (const std::string& to : p.links) {
            if (g.index.count(to)) f << p.url << " -> " << to << "\n";
        }
    }
    f.close();
    return f.good();
}

int failed = 0;

void check(bool ok, const std::string& name) {
    if (!ok) {
        failed++;
        std::cout << "FAIL " << name << "\n";
    }
}

void fakeCrawl(std::map<std::string, std::string>& site, const std::string& start,
               int maxDepth, int maxPages, Graph& g) {
    g.add(start, 0);
    std::vector<std::string> level{start};
    for (int depth = 0; depth <= maxDepth && !level.empty(); depth++) {
        std::vector<std::string> next;
        for (const std::string& url : level) {
            int page = g.index[url];
            g.pages[page].status = site.count(url) ? "200" : "404";
            if (site.count(url)) g.links(page, site[url], maxDepth, maxPages, next);
        }
        level = next;
    }
}

void tests() {
    Url base;
    parseUrl("http://site.ru/a/b/page.html", base);
    check(normalizeLink(base, "c.html") == "http://site.ru/a/b/c.html", "относительная ссылка");
    check(normalizeLink(base, "../c.html") == "http://site.ru/a/c.html", "переход вверх");
    check(normalizeLink(base, "/c.html#x") == "http://site.ru/c.html", "корень и фрагмент");
    check(normalizeLink(base, "http://SITE.ru:80/a/../c.html") == "http://site.ru/c.html", "регистр, порт, путь");
    check(normalizeLink(base, "http://other.ru/a") == "" && normalizeLink(base, "mailto:a@b") == "", "чужое отброшено");

    std::vector<std::string> links = extractLinks("<A HREF='a.html'>1</A><a href=\"b.html\">2</a><link href='s.css'>");
    check(links.size() == 2 && links[0] == "a.html" && links[1] == "b.html", "извлечение ссылок");

    std::map<std::string, std::string> site;
    site["http://s.ru/index.html"] = "<a href='a.html'>a</a><a href='b.html'>b</a><a href='a.html'>x</a>";
    site["http://s.ru/a.html"] = "<a href='sub/c.html'>c</a>";
    site["http://s.ru/b.html"] = "<a href='index.html'>i</a>";
    site["http://s.ru/sub/c.html"] = "<a href='../index.html'>i</a>";

    Graph g;
    fakeCrawl(site, "http://s.ru/index.html", 2, 10, g);
    check(g.pages.size() == 4 && g.pages[g.index["http://s.ru/sub/c.html"]].depth == 2, "эталонный граф");
    check(g.pages[0].links.size() == 2, "дубликаты ссылок убраны");

    Graph g2;
    fakeCrawl(site, "http://s.ru/index.html", 1, 10, g2);
    check(g2.pages.size() == 3, "ограничение глубины");

    Graph g3;
    fakeCrawl(site, "http://s.ru/index.html", 5, 2, g3);
    check(g3.pages.size() == 2, "ограничение страниц");

    std::error_code ec;
    fs::create_directories("site/sub", ec);
    std::ofstream("site/index.html") << "<a href='a.html'>a</a> <a href='sub/c.html'>c</a>";
    std::ofstream("site/a.html") << "<a href='index.html'>home</a>";
    std::ofstream("site/sub/c.html") << "<a href='../index.html'>home</a>";
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "test") {
        tests();
        std::cout << (failed ? "есть ошибки\n" : "все тесты пройдены\n");
        return failed ? 1 : 0;
    }
    if (argc != 8) {
        std::cout << "lab2 sync|async url глубина страниц запросы таймаут граф\n";
        return 1;
    }

    std::string mode = argv[1];
    int maxDepth = atoi(argv[3]), maxPages = atoi(argv[4]);
    int conc = atoi(argv[5]), timeout = atoi(argv[6]);
    if ((mode != "sync" && mode != "async") || maxDepth < 0 || maxPages < 1 || conc < 1 || conc > 64 || timeout < 1) return 1;

    auto start = std::chrono::steady_clock::now();
    std::cout << "начало " << now() << ", режим " << mode << "\n";

    Graph g;
    crawl(argv[2], maxDepth, maxPages, conc, timeout, mode == "async", g);
    std::cout << "обход мс " << ms(start) << "\n";
    for (const std::string& s : order) std::cout << s << "\n";

    std::cout << (saveGraph(argv[7], g) ? "граф сохранён" : "ошибка записи");
    std::cout << ", страниц " << g.pages.size() << "\n";
    std::cout << "конец " << now() << ", всего мс " << ms(start) << "\n";
    return 0;
}