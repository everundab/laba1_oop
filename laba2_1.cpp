#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstdio>
#include <ctime>

struct Source {
    int x, y;
    long long r, a;
};

long long addSat(long long a, long long b) {
    return a > LLONG_MAX - b ? LLONG_MAX : a + b;
}

long long contrib(const Source& s, int x, int y) {
    long long k = s.r + 1 - std::abs((long long)x - s.x) - std::abs((long long)y - s.y);
    if (k <= 0) return 0;
    return s.a > LLONG_MAX / k ? LLONG_MAX : s.a * k;
}

std::string now() {
    std::time_t t = std::time(0);
    char b[32];
    std::strftime(b, 32, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return b;
}

struct Map {
    int w, h;
    std::vector<long long> cells;
    std::vector<Source> sources;
    std::mutex mtx;

    Map(int width, int height) : w(width), h(height), cells((size_t)width * height, 0) {}

    void addRange(int from, int to, bool sync) {
        for (int i = from; i < to; i++) {
            Source s = sources[i];
            long long x1 = std::max(0LL, s.x - s.r);
            long long x2 = std::min((long long)w - 1, s.x + s.r);
            long long y1 = std::max(0LL, s.y - s.r);
            long long y2 = std::min((long long)h - 1, s.y + s.r);

            for (long long y = y1; y <= y2; y++) {
                for (long long x = x1; x <= x2; x++) {
                    long long v = contrib(s, (int)x, (int)y);
                    size_t k = (size_t)(y * w + x);
                    if (sync) mtx.lock();
                    cells[k] = addSat(cells[k], v);
                    if (sync) mtx.unlock();
                }
            }
        }
    }

    void build(int mode, int threads) {
        int n = (int)sources.size();
        int parts = mode == 1 ? 1 : std::min(threads, std::max(n, 1));
        auto start = std::chrono::steady_clock::now();
        std::vector<std::thread> pool;

        for (int i = 0; i < parts; i++) {
            int from = (int)((long long)n * i / parts);
            int to = (int)((long long)n * (i + 1) / parts);
            if (parts == 1) addRange(from, to, false);
            else pool.push_back(std::thread(&Map::addRange, this, from, to, mode == 3));
        }
        for (std::thread& t : pool) t.join();

        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::cout << "потоков " << parts << ", вычисление мс " << ms << "\n";
    }
};

std::vector<long long> reference(const Map& m) {
    std::vector<long long> out(m.cells.size(), 0);
    for (int y = 0; y < m.h; y++) {
        for (int x = 0; x < m.w; x++) {
            long long sum = 0;
            for (const Source& s : m.sources) sum = addSat(sum, contrib(s, x, y));
            out[(size_t)y * m.w + x] = sum;
        }
    }
    return out;
}

void save(const Map& m, long long mx, const std::string& matrixPath, const std::string& ppmPath) {
    std::ofstream t(matrixPath);
    for (int y = 0; y < m.h; y++) {
        for (int x = 0; x < m.w; x++) t << m.cells[(size_t)y * m.w + x] << " ";
        t << "\n";
    }

    std::ofstream p(ppmPath, std::ios::binary);
    p << "P6\n" << m.w << " " << m.h << "\n255\n";
    for (long long v : m.cells) {
        char rgb[3] = {(char)(mx > 0 ? 255.0 * v / mx : 0), 0, 0};
        p.write(rgb, 3);
    }
}

int failed = 0;

void check(bool ok, const std::string& name) {
    if (!ok) {
        failed++;
        std::cout << "FAIL " << name << "\n";
    }
}

void tests() {
    Source s{2, 2, 2, 10};
    check(contrib(s, 2, 2) == 30 && contrib(s, 4, 2) == 10 && contrib(s, 5, 2) == 0, "формула вклада");
    check(contrib(s, 3, 3) == 10, "манхэттенское расстояние");
    check(addSat(LLONG_MAX - 5, 10) == LLONG_MAX, "насыщение суммы");
    check(contrib(Source{0, 0, 1000000000, 1000000000000000000LL}, 0, 0) == LLONG_MAX, "насыщение произведения");

    Map m(20, 15);
    m.sources = {{5, 5, 4, 10}, {8, 6, 6, 7}, {-3, -3, 5, 20}, {100, 100, 2, 50}};
    std::vector<long long> ref = reference(m);

    for (int threads : {1, 2, 4, 7}) {
        Map a(20, 15), b(20, 15);
        a.sources = m.sources;
        b.sources = m.sources;
        a.build(1, threads);
        b.build(3, threads);
        check(a.cells == ref && b.cells == ref, "режимы 1 и 3 равны эталону");
    }

    Map empty(5, 5);
    empty.build(3, 4);
    check(empty.cells == std::vector<long long>(25, 0), "без источников нули");

    Map pic(2, 2);
    pic.cells = {10, 5, 0, 10};
    save(pic, 10, "t.txt", "t.ppm");
    std::ifstream f("t.ppm", std::ios::binary);
    std::string head(11, ' ');
    char rgb[3];
    f.read(&head[0], 11);
    f.read(rgb, 3);
    f.close();
    check(head == "P6\n2 2\n255\n" && (unsigned char)rgb[0] == 255 && rgb[1] == 0, "PPM и цвет");
    std::remove("t.txt");
    std::remove("t.ppm");
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "test") {
        tests();
        std::cout << (failed ? "есть ошибки\n" : "все тесты пройдены\n");
        return failed ? 1 : 0;
    }
    if (argc < 9 || (argc - 9) % 4 != 0) {
        std::cout << "lab1 режим потоки w h qx qy матрица карта [sx sy R A ...]\n";
        return 1;
    }

    int mode = atoi(argv[1]), threads = atoi(argv[2]);
    int w = atoi(argv[3]), h = atoi(argv[4]);
    if (mode < 1 || mode > 3 || threads < 1 || threads > 64 || w < 1 || h < 1) return 1;

    auto start = std::chrono::steady_clock::now();
    std::cout << "начало " << now() << ", режим " << mode << "\n";

    Map m(w, h);
    for (int i = 9; i < argc; i += 4) {
        Source s{atoi(argv[i]), atoi(argv[i + 1]), atoll(argv[i + 2]), atoll(argv[i + 3])};
        if (s.r < 0 || s.a < 0 || s.r > 1000000000LL || s.a > 1000000000000000000LL) return 1;
        m.sources.push_back(s);
    }
    m.build(mode, threads);

    long long mn = m.cells[0], mx = m.cells[0];
    for (long long v : m.cells) {
        mn = std::min(mn, v);
        mx = std::max(mx, v);
    }
    std::cout << "минимум " << mn << ", максимум " << mx << "\n";

    int qx = atoi(argv[5]), qy = atoi(argv[6]);
    if (qx >= 0 && qx < w && qy >= 0 && qy < h) {
        std::cout << "клетка (" << qx << "," << qy << ") = " << m.cells[(size_t)qy * w + qx] << "\n";
    }

    save(m, mx, argv[7], argv[8]);
    std::cout << "сохранено: " << argv[7] << " и " << argv[8] << "\n";

    if (m.cells.size() * (m.sources.size() + 1) <= 200000000ULL) {
        std::vector<long long> ref = reference(m);
        int diff = 0;
        for (size_t i = 0; i < m.cells.size(); i++) if (m.cells[i] != ref[i]) diff++;
        std::cout << "отличие от эталона, клеток: " << diff << "\n";
    }

    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << "конец " << now() << ", всего мс " << ms << "\n";
    return 0;
}