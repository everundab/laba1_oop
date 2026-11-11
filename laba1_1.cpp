#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <stdexcept>

using namespace std;

class Vector2D {
private:
    int dx;
    int dy;

public:
    Vector2D(int x = 0, int y = 0) {
        dx = x;
        dy = y;
    }

    int getDx() const { return dx; }
    int getDy() const { return dy; }

    Vector2D operator+(const Vector2D& other) const {
        return Vector2D(dx + other.dx, dy + other.dy);
    }

    Vector2D operator-(const Vector2D& other) const {
        return Vector2D(dx - other.dx, dy - other.dy);
    }

    bool operator==(const Vector2D& other) const {
        return dx == other.dx && dy == other.dy;
    }

    bool operator!=(const Vector2D& other) const {
        return !(*this == other);
    }
};

class Point2D {
private:
    int x;
    int y;

public:
    Point2D(int px = 0, int py = 0) {
        x = px;
        y = py;
    }

    int getX() const { return x; }
    int getY() const { return y; }

    Point2D operator+(const Vector2D& v) const {
        return Point2D(x + v.getDx(), y + v.getDy());
    }

    Vector2D operator-(const Point2D& other) const {
        return Vector2D(x - other.x, y - other.y);
    }

    bool operator==(const Point2D& other) const {
        return x == other.x && y == other.y;
    }

    bool operator!=(const Point2D& other) const {
        return !(*this == other);
    }
};

class Rect {
private:
    Point2D pos;
    Vector2D size;

public:
    Rect(Point2D p, Vector2D s) {
        if (s.getDx() < 0 || s.getDy() < 0) {
            throw invalid_argument("Negative size");
        }
        pos = p;
        size = s;
    }

    Point2D getPos() const { return pos; }
    Vector2D getSize() const { return size; }

    bool contains(const Point2D& p) const {
        bool inX = p.getX() >= pos.getX() && p.getX() < pos.getX() + size.getDx();
        bool inY = p.getY() >= pos.getY() && p.getY() < pos.getY() + size.getDy();
        return inX && inY;
    }

    bool operator==(const Rect& other) const {
        return pos == other.pos && size == other.size;
    }

    bool operator!=(const Rect& other) const {
        return !(*this == other);
    }
};

class UIElement {
private:
    Point2D pos;
    Vector2D size;
    bool enabled;

public:
    UIElement(Point2D p, Vector2D s, bool en = true) {
        pos = p;
        size = s;
        enabled = en;
    }

    virtual ~UIElement() {}

    Rect getBounds() const {
        return Rect(pos, size);
    }

    bool containsPoint(const Point2D& p) const {
        return getBounds().contains(p);
    }

    bool isEnabled() const { return enabled; }
    void setEnabled(bool en) { enabled = en; }

    virtual void onClick() = 0;
    virtual string getText() const = 0;

    virtual bool canFocus() const {
        return false;
    }

    virtual void inputText(const string& t) {
    }
};

class Button : public UIElement {
private:
    string title;
    int clicks;

public:
    Button(Point2D p, Vector2D s, string t, bool en = true)
        : UIElement(p, s, en) {
        title = t;
        clicks = 0;
    }

    void onClick() override {
        if (isEnabled()) {
            clicks++;
        }
    }

    int getClicks() const { return clicks; }

    string getText() const override {
        return title + ":" + to_string(clicks);
    }
};

class CheckBox : public UIElement {
private:
    string title;
    bool checked;

public:
    CheckBox(Point2D p, Vector2D s, string t, bool ch = false, bool en = true)
        : UIElement(p, s, en) {
        title = t;
        checked = ch;
    }

    void onClick() override {
        if (isEnabled()) {
            checked = !checked;
        }
    }

    bool isChecked() const { return checked; }

    string getText() const override {
        if (checked) {
            return "[x] " + title;
        }
        return "[ ] " + title;
    }
};

class TextField : public UIElement {
private:
    string content;

public:
    TextField(Point2D p, Vector2D s, string text = "", bool en = true)
        : UIElement(p, s, en) {
        content = text;
    }

    void onClick() override {
    }

    bool canFocus() const override {
        return isEnabled();
    }

    void inputText(const string& t) override {
        if (isEnabled()) {
            content = t;
        }
    }

    string getText() const override {
        return content;
    }
};

class Form {
private:
    vector<shared_ptr<UIElement>> items;
    int focusIndex;

    int findTopIndex(const Point2D& p) const {
        for (int i = (int)items.size() - 1; i >= 0; i--) {
            if (items[i]->containsPoint(p)) {
                return i;
            }
        }
        return -1;
    }

public:
    Form() {
        focusIndex = -1;
    }

    void addElement(shared_ptr<UIElement> el) {
        items.push_back(el);
    }

    void removeElement(shared_ptr<UIElement> el) {
        for (int i = 0; i < (int)items.size(); i++) {
            if (items[i] == el) {
                items.erase(items.begin() + i);
                if (focusIndex == i) {
                    focusIndex = -1;
                } else if (focusIndex > i) {
                    focusIndex--;
                }
                break;
            }
        }
    }

    void setElementEnabled(shared_ptr<UIElement> el, bool en) {
        for (int i = 0; i < (int)items.size(); i++) {
            if (items[i] == el) {
                items[i]->setEnabled(en);
                if (!en && focusIndex == i) {
                    focusIndex = -1;
                }
                break;
            }
        }
    }

    shared_ptr<UIElement> getElementAt(const Point2D& p) const {
        int idx = findTopIndex(p);
        if (idx == -1) return nullptr;
        return items[idx];
    }

    void clickAt(const Point2D& p) {
        int idx = findTopIndex(p);
        if (idx == -1) return;

        if (!items[idx]->isEnabled()) {
            return;
        }

        items[idx]->onClick();
        if (items[idx]->canFocus()) {
            focusIndex = idx;
        }
    }

    void sendText(const string& text) {
        if (focusIndex != -1) {
            items[focusIndex]->inputText(text);
        }
    }

    void nextFocus() {
        int n = (int)items.size();
        if (n == 0) return;

        int start = focusIndex + 1;
        for (int step = 0; step < n; step++) {
            int curr = (start + step) % n;
            if (items[curr]->isEnabled() && items[curr]->canFocus()) {
                focusIndex = curr;
                return;
            }
        }
        focusIndex = -1;
    }

    shared_ptr<UIElement> getFocused() const {
        if (focusIndex == -1) return nullptr;
        return items[focusIndex];
    }

    void draw(int width, int height) const {
        vector<string> screen(height, string(width, ' '));

        for (int i = 0; i < (int)items.size(); i++) {
            Rect r = items[i]->getBounds();
            int x0 = r.getPos().getX();
            int y0 = r.getPos().getY();
            int w = r.getSize().getDx();
            int h = r.getSize().getDy();
            bool focused = (i == focusIndex);

            for (int y = y0; y < y0 + h && y < height; y++) {
                for (int x = x0; x < x0 + w && x < width; x++) {
                    if (x < 0 || y < 0) continue;

                    bool borderY = (y == y0 || y == y0 + h - 1);
                    bool borderX = (x == x0 || x == x0 + w - 1);

                    if (focused) {
                        if (borderX || borderY) screen[y][x] = '#';
                        else screen[y][x] = ' ';
                    } else {
                        if (borderX && borderY) screen[y][x] = '+';
                        else if (borderY) screen[y][x] = '-';
                        else if (borderX) screen[y][x] = '|';
                        else screen[y][x] = ' ';
                    }
                }
            }

            string txt = items[i]->getText();
            int ty = y0 + h / 2;
            int tx = x0 + 1;
            if (ty > y0 && ty < y0 + h - 1 && ty < height) {
                for (int c = 0; c < (int)txt.size(); c++) {
                    if (tx + c < x0 + w - 1 && tx + c < width) {
                        screen[ty][tx + c] = txt[c];
                    }
                }
            }
        }

        for (int y = 0; y < height; y++) {
            cout << screen[y] << "\n";
        }
    }
};

void runTests() {
    Point2D p1(2, 3);
    Vector2D v1(4, 5);
    Point2D p2 = p1 + v1;
    assert(p2 == Point2D(6, 8));
    assert((p2 - p1) == v1);

    Rect r(Point2D(0, 0), Vector2D(10, 5));
    assert(r.contains(Point2D(5, 2)) == true);
    assert(r.contains(Point2D(10, 5)) == false);

    auto b = make_shared<Button>(Point2D(0, 0), Vector2D(18, 5), "Button");
    auto c = make_shared<CheckBox>(Point2D(20, 0), Vector2D(18, 5), "CheckBox");
    auto t1 = make_shared<TextField>(Point2D(0, 7), Vector2D(22, 5), "");
    auto t2 = make_shared<TextField>(Point2D(24, 7), Vector2D(18, 5), "TextField");

    Form f;
    f.addElement(b);
    f.addElement(c);
    f.addElement(t1);
    f.addElement(t2);

    f.clickAt(Point2D(2, 2));
    assert(b->getClicks() == 1);

    f.setElementEnabled(b, false);
    f.clickAt(Point2D(2, 2));
    assert(b->getClicks() == 1);
    f.setElementEnabled(b, true);

    f.clickAt(Point2D(22, 2));
    assert(c->isChecked() == true);

    f.clickAt(Point2D(2, 8));
    assert(f.getFocused() == t1);
    f.sendText("Alice");
    assert(t1->getText() == "Alice");

    f.nextFocus();
    assert(f.getFocused() == t2);
    f.nextFocus();
    assert(f.getFocused() == t1);

    auto topB = make_shared<Button>(Point2D(0, 0), Vector2D(10, 5), "Top");
    f.addElement(topB);
    f.clickAt(Point2D(2, 2));
    assert(topB->getClicks() == 1);
    assert(b->getClicks() == 1);

    cout << "Тесты Задания 1 пройдены успешно!\n\n";
}

int main() {
    runTests();

    Form form;
    auto b = make_shared<Button>(Point2D(0, 0), Vector2D(18, 5), "Button");
    auto c = make_shared<CheckBox>(Point2D(20, 0), Vector2D(18, 5), "CheckBox");
    auto t1 = make_shared<TextField>(Point2D(0, 7), Vector2D(22, 5), "");
    auto t2 = make_shared<TextField>(Point2D(25, 7), Vector2D(20, 5), "TextField");

    form.addElement(b);
    form.addElement(c);
    form.addElement(t1);
    form.addElement(t2);

    form.clickAt(Point2D(2, 2));
    form.clickAt(Point2D(22, 2));
    form.clickAt(Point2D(2, 8));
    form.sendText("Alice");

    form.draw(46, 13);

    return 0;
}