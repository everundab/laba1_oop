#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <stdexcept>

// ==========================================
// 1. ИММУТАБЕЛЬНЫЕ ГЕОМЕТРИЧЕСКИЕ ТИПЫ
// ==========================================

class Vector2D {
private:
    int dx_;
    int dy_;

public:
    Vector2D(int dx = 0, int dy = 0) : dx_(dx), dy_(dy) {}

    int getDx() const { return dx_; }
    int getDy() const { return dy_; }

    Vector2D operator+(const Vector2D& other) const {
        return Vector2D(dx_ + other.dx_, dy_ + other.dy_);
    }

    Vector2D operator-(const Vector2D& other) const {
        return Vector2D(dx_ - other.dx_, dy_ - other.dy_);
    }

    bool operator==(const Vector2D& other) const {
        return dx_ == other.dx_ && dy_ == other.dy_;
    }

    bool operator!=(const Vector2D& other) const {
        return !(*this == other);
    }
};

class Point2D {
private:
    int x_;
    int y_;

public:
    Point2D(int x = 0, int y = 0) : x_(x), y_(y) {}

    int getX() const { return x_; }
    int getY() const { return y_; }

    Point2D operator+(const Vector2D& vec) const {
        return Point2D(x_ + vec.getDx(), y_ + vec.getDy());
    }

    Vector2D operator-(const Point2D& other) const {
        return Vector2D(x_ - other.x_, y_ - other.y_);
    }

    bool operator==(const Point2D& other) const {
        return x_ == other.x_ && y_ == other.y_;
    }

    bool operator!=(const Point2D& other) const {
        return !(*this == other);
    }
};

class Rect {
private:
    Point2D topLeft_;
    Vector2D size_; // dx - ширина, dy - высота

public:
    Rect(Point2D topLeft, Vector2D size) : topLeft_(topLeft), size_(size) {
        if (size.getDx() < 0 || size.getDy() < 0) {
            throw std::invalid_argument("Размер прямоугольника не может быть отрицательным");
        }
    }

    Point2D getPosition() const { return topLeft_; }
    Vector2D getSize() const { return size_; }
    int getWidth() const { return size_.getDx(); }
    int getHeight() const { return size_.getDy(); }

    bool contains(const Point2D& p) const {
        return p.getX() >= topLeft_.getX() &&
               p.getX() < topLeft_.getX() + size_.getDx() &&
               p.getY() >= topLeft_.getY() &&
               p.getY() < topLeft_.getY() + size_.getDy();
    }

    bool operator==(const Rect& other) const {
        return topLeft_ == other.topLeft_ && size_ == other.size_;
    }

    bool operator!=(const Rect& other) const {
        return !(*this == other);
    }
};

// ==========================================
// 2. ИЕРАРХИЯ ЭЛЕМЕНТОВ ИНТЕРФЕЙСА
// ==========================================

class UIElement {
private:
    Point2D position_;
    Vector2D size_;
    bool enabled_;

protected:
    // Вызывается только если элемент enabled
    virtual void onClickAction() = 0;

public:
    UIElement(Point2D pos, Vector2D size, bool enabled = true)
        : position_(pos), size_(size), enabled_(enabled) {}

    virtual ~UIElement() = default;

    Rect getBounds() const {
        return Rect(position_, size_);
    }

    bool containsPoint(const Point2D& p) const {
        return getBounds().contains(p);
    }

    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    // Шаблонный метод обработки нажатия
    void onClick() {
        if (enabled_) {
            onClickAction();
        }
    }

    // Полиморфное определение возможности фокуса (без if/dynamic_cast в форме)
    virtual bool canFocus() const {
        return false;
    }

    // Полиморфная обработка ввода текста
    virtual void onTextInput(const std::string& text) {
        // По умолчанию элементы игнорируют ввод текста
        (void)text;
    }

    virtual std::string getTextRepresentation() const = 0;
};

// Кнопка
class Button : public UIElement {
private:
    std::string label_;
    int clickCount_;

protected:
    void onClickAction() override {
        clickCount_++;
    }

public:
    Button(Point2D pos, Vector2D size, const std::string& label, bool enabled = true)
        : UIElement(pos, size, enabled), label_(label), clickCount_(0) {}

    int getClickCount() const { return clickCount_; }

    std::string getTextRepresentation() const override {
        return label_ + ":" + std::to_string(clickCount_);
    }
};

// Флажок (CheckBox)
class CheckBox : public UIElement {
private:
    std::string label_;
    bool checked_;

protected:
    void onClickAction() override {
        checked_ = !checked_;
    }

public:
    CheckBox(Point2D pos, Vector2D size, const std::string& label, bool checked = false, bool enabled = true)
        : UIElement(pos, size, enabled), label_(label), checked_(checked) {}

    bool isChecked() const { return checked_; }

    std::string getTextRepresentation() const override {
        return std::string(checked_ ? "[x] " : "[ ] ") + label_;
    }
};

// Текстовое поле (TextField)
class TextField : public UIElement {
private:
    std::string text_;

protected:
    void onClickAction() override {
        // Само получение фокуса координируется формой через canFocus()
    }

public:
    TextField(Point2D pos, Vector2D size, const std::string& initialText = "", bool enabled = true)
        : UIElement(pos, size, enabled), text_(initialText) {}

    bool canFocus() const override {
        return isEnabled();
    }

    void onTextInput(const std::string& text) override {
        if (isEnabled()) {
            text_ = text;
        }
    }

    std::string getText() const { return text_; }

    std::string getTextRepresentation() const override {
        return text_;
    }
};

// ==========================================
// 3. КЛАСС ФОРМЫ
// ==========================================

class Form {
private:
    std::vector<std::shared_ptr<UIElement>> elements_;
    int focusedIndex_; // -1, если фокуса нет

    // Единый механизм поиска верхнего элемента под точкой (с учётом наложения)
    int findTopElementIndexAt(const Point2D& p) const {
        for (int i = static_cast<int>(elements_.size()) - 1; i >= 0; --i) {
            if (elements_[i]->containsPoint(p)) {
                return i;
            }
        }
        return -1;
    }

public:
    Form() : focusedIndex_(-1) {}

    void addElement(std::shared_ptr<UIElement> elem) {
        if (elem) {
            elements_.push_back(elem);
        }
    }

    void removeElement(std::shared_ptr<UIElement> elem) {
        for (size_t i = 0; i < elements_.size(); ++i) {
            if (elements_[i] == elem) {
                elements_.erase(elements_.begin() + i);
                if (focusedIndex_ == static_cast<int>(i)) {
                    focusedIndex_ = -1;
                } else if (focusedIndex_ > static_cast<int>(i)) {
                    focusedIndex_--;
                }
                return;
            }
        }
    }

    void setElementEnabled(std::shared_ptr<UIElement> elem, bool enabled) {
        for (size_t i = 0; i < elements_.size(); ++i) {
            if (elements_[i] == elem) {
                elements_[i]->setEnabled(enabled);
                // Disabled элемент не может удерживать фокус
                if (!enabled && focusedIndex_ == static_cast<int>(i)) {
                    focusedIndex_ = -1;
                }
                return;
            }
        }
    }

    // Определение верхнего элемента под точкой
    std::shared_ptr<UIElement> getElementAt(const Point2D& p) const {
        int idx = findTopElementIndexAt(p);
        if (idx != -1) {
            return elements_[idx];
        }
        return nullptr;
    }

    // Обработка нажатия по координатам (использует тот же findTopElementIndexAt)
    void clickAt(const Point2D& p) {
        int idx = findTopElementIndexAt(p);
        if (idx == -1) return;

        auto elem = elements_[idx];
        if (!elem->isEnabled()) {
            return; // Нажатие на disabled не меняет состояние и фокус
        }

        elem->onClick();

        if (elem->canFocus()) {
            focusedIndex_ = idx;
        }
    }

    // Передача текстового ввода элементу в фокусе
    void sendTextInput(const std::string& text) {
        if (focusedIndex_ != -1 && focusedIndex_ < static_cast<int>(elements_.size())) {
            elements_[focusedIndex_]->onTextInput(text);
        }
    }

    // Перевод фокуса на следующий доступный элемент по кругу
    void focusNext() {
        if (elements_.empty()) return;

        int n = static_cast<int>(elements_.size());
        int start = (focusedIndex_ == -1) ? 0 : (focusedIndex_ + 1) % n;

        for (int step = 0; step < n; ++step) {
            int idx = (start + step) % n;
            // Проверяем полиморфно без проверки конкретного типа
            if (elements_[idx]->isEnabled() && elements_[idx]->canFocus()) {
                focusedIndex_ = idx;
                return;
            }
        }
        focusedIndex_ = -1;
    }

    std::shared_ptr<UIElement> getFocusedElement() const {
        if (focusedIndex_ != -1 && focusedIndex_ < static_cast<int>(elements_.size())) {
            return elements_[focusedIndex_];
        }
        return nullptr;
    }

    // Отрисовка формы в консоль
    void render(int canvasWidth, int canvasHeight) const {
        std::vector<std::string> canvas(canvasHeight, std::string(canvasWidth, ' '));

        for (size_t i = 0; i < elements_.size(); ++i) {
            Rect r = elements_[i]->getBounds();
            int x0 = r.getPosition().getX();
            int y0 = r.getPosition().getY();
            int w = r.getWidth();
            int h = r.getHeight();
            bool hasFocus = (static_cast<int>(i) == focusedIndex_);

            if (w < 2 || h < 2) continue;

            // Отрисовка фона и рамок элемента
            for (int y = y0; y < y0 + h && y < canvasHeight; ++y) {
                if (y < 0) continue;
                for (int x = x0; x < x0 + w && x < canvasWidth; ++x) {
                    if (x < 0) continue;

                    bool isTopOrBottom = (y == y0 || y == y0 + h - 1);
                    bool isLeftOrRight = (x == x0 || x == x0 + w - 1);

                    if (hasFocus) {
                        if (isTopOrBottom || isLeftOrRight) canvas[y][x] = '#';
                        else canvas[y][x] = ' ';
                    } else {
                        if (isTopOrBottom && isLeftOrRight) canvas[y][x] = '+';
                        else if (isTopOrBottom) canvas[y][x] = '-';
                        else if (isLeftOrRight) canvas[y][x] = '|';
                        else canvas[y][x] = ' ';
                    }
                }
            }

            // Вывод текста по центру высоты внутри рамки
            std::string text = elements_[i]->getTextRepresentation();
            int textY = y0 + h / 2;
            int textX = x0 + 1;
            if (textY > y0 && textY < y0 + h - 1 && textY >= 0 && textY < canvasHeight) {
                for (size_t c = 0; c < text.size(); ++c) {
                    int curX = textX + static_cast<int>(c);
                    if (curX < x0 + w - 1 && curX >= 0 && curX < canvasWidth) {
                        canvas[textY][curX] = text[c];
                    }
                }
            }
        }

        for (const auto& line : canvas) {
            std::cout << line << "\n";
        }
    }
};

// ==========================================
// 4. АВТОМАТИЧЕСКИЕ ТЕСТЫ И MAIN
// ==========================================

void runTests() {
    // 1. Тест геометрии и операторов
    Point2D p1(2, 3);
    Vector2D v1(5, 7);
    Point2D p2 = p1 + v1;
    assert(p2 == Point2D(7, 10));
    assert((p2 - p1) == v1);
    assert(p1 != p2);

    Vector2D v2(1, 2);
    assert((v1 + v2) == Vector2D(6, 9));
    assert((v1 - v2) == Vector2D(4, 5));

    Rect rect(Point2D(0, 0), Vector2D(10, 5));
    assert(rect.contains(Point2D(0, 0)));
    assert(rect.contains(Point2D(9, 4)));
    assert(!rect.contains(Point2D(10, 5))); // Правая/нижняя граница не включена

    // 2. Тест элементов и их состояний
    auto btn = std::make_shared<Button>(Point2D(0, 0), Vector2D(18, 5), "Button");
    auto chk = std::make_shared<CheckBox>(Point2D(20, 0), Vector2D(18, 5), "CheckBox");
    auto tf1 = std::make_shared<TextField>(Point2D(0, 7), Vector2D(22, 5), "");
    auto tf2 = std::make_shared<TextField>(Point2D(24, 7), Vector2D(18, 5), "TextField");

    Form form;
    form.addElement(btn);
    form.addElement(chk);
    form.addElement(tf1);
    form.addElement(tf2);

    // Нажатия на кнопку и чекбокс
    form.clickAt(Point2D(2, 2));
    assert(btn->getClickCount() == 1);
    assert(form.getFocusedElement() == nullptr); // Кнопка не берет фокус

    form.clickAt(Point2D(22, 2));
    assert(chk->isChecked() == true);
    form.clickAt(Point2D(22, 2));
    assert(chk->isChecked() == false);

    // Проверка disabled состояния
    form.setElementEnabled(btn, false);
    form.clickAt(Point2D(2, 2));
    assert(btn->getClickCount() == 1); // Счётчик не увеличился
    form.setElementEnabled(btn, true);

    // 3. Тест фокуса и ввода текста
    form.clickAt(Point2D(2, 8)); // Клик по первому TextField
    assert(form.getFocusedElement() == tf1);
    form.sendTextInput("Alice");
    assert(tf1->getText() == "Alice");

    // Переключение фокуса (должно пропустить кнопку и чекбокс и перейти на tf2)
    form.focusNext();
    assert(form.getFocusedElement() == tf2);

    // Циклический переход обратно на tf1
    form.focusNext();
    assert(form.getFocusedElement() == tf1);

    // Если отключить tf2, фокус должен оставаться только на tf1
    form.setElementEnabled(tf2, false);
    form.focusNext();
    assert(form.getFocusedElement() == tf1);
    form.setElementEnabled(tf2, true);

    // 4. Тест порядка наложения (Z-order)
    auto topBtn = std::make_shared<Button>(Point2D(0, 0), Vector2D(10, 5), "Top");
    form.addElement(topBtn); // Добавлен позже поверх btn
    assert(form.getElementAt(Point2D(2, 2)) == topBtn);
    form.clickAt(Point2D(2, 2));
    assert(topBtn->getClickCount() == 1);
    assert(btn->getClickCount() == 1); // Нижняя кнопка клик не получила

    form.removeElement(topBtn);
    assert(form.getElementAt(Point2D(2, 2)) == btn);

    std::cout << "[OK] Все автоматические тесты Задания 1 успешно пройдены!\n\n";
}

int main() {
    runTests();

    // Демонстрация вывода в консоль (в точности как в примере из методички)
    Form demoForm;
    auto btn = std::make_shared<Button>(Point2D(0, 0), Vector2D(18, 5), "Button");
    auto chk = std::make_shared<CheckBox>(Point2D(20, 0), Vector2D(18, 5), "CheckBox");
    auto tfAlice = std::make_shared<TextField>(Point2D(0, 7), Vector2D(22, 5), "");
    auto tfOther = std::make_shared<TextField>(Point2D(25, 7), Vector2D(20, 5), "TextField");

    demoForm.addElement(btn);
    demoForm.addElement(chk);
    demoForm.addElement(tfAlice);
    demoForm.addElement(tfOther);

    demoForm.clickAt(Point2D(2, 2));  // Нажимаем Button -> Button:1
    demoForm.clickAt(Point2D(22, 2)); // Нажимаем CheckBox -> [x] CheckBox
    demoForm.clickAt(Point2D(2, 8));  // Фокус на первое текстовое поле
    demoForm.sendTextInput("Alice");  // Вводим текст "Alice"

    std::cout << "Демонстрация отрисовки формы (фокус на поле Alice):\n\n";
    demoForm.render(46, 13);

    return 0;
}