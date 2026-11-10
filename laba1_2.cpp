#include <iostream>
#include <string>
#include <memory>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <functional>

// ==========================================
// 1. ПРЕДСТАВЛЕНИЕ ПРЕДМЕТНОЙ ОБЛАСТИ И СТРУКТУРЫ
// ==========================================

enum class NodeKind {
    Constant,
    Variable,
    Binary
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div
};

// Шаблонный класс узла дерева выражений (иммутабельный)
template <typename T>
class ExprNode {
public:
    using Ptr = std::shared_ptr<const ExprNode<T>>;

private:
    NodeKind kind_;
    T value_;
    std::string varName_;
    BinaryOp op_;
    Ptr left_;
    Ptr right_;

    // Приватные конструкторы для защиты инвариантов структуры
    ExprNode(T val)
        : kind_(NodeKind::Constant), value_(val), varName_(""), op_(BinaryOp::Add), left_(nullptr), right_(nullptr) {}

    ExprNode(const std::string& name)
        : kind_(NodeKind::Variable), value_(T{}), varName_(name), op_(BinaryOp::Add), left_(nullptr), right_(nullptr) {
        if (name.empty()) {
            throw std::invalid_argument("Имя переменной не может быть пустым");
        }
    }

    ExprNode(BinaryOp op, Ptr left, Ptr right)
        : kind_(NodeKind::Binary), value_(T{}), varName_(""), op_(op), left_(left), right_(right) {
        // Инвариант: бинарная операция обязана иметь ровно два непустых операнда
        if (!left || !right) {
            throw std::invalid_argument("Бинарная операция должна содержать ровно два непустых операнда");
        }
    }

public:
    // Фабричные методы создания узлов
    static Ptr makeConst(T val) {
        return Ptr(new ExprNode<T>(val));
    }

    static Ptr makeVar(const std::string& name) {
        return Ptr(new ExprNode<T>(name));
    }

    static Ptr makeBinary(BinaryOp op, Ptr left, Ptr right) {
        return Ptr(new ExprNode<T>(op, left, right));
    }

    NodeKind getKind() const { return kind_; }
    T getValue() const { return value_; }
    const std::string& getVarName() const { return varName_; }
    BinaryOp getOp() const { return op_; }
    Ptr getLeft() const { return left_; }
    Ptr getRight() const { return right_; }
};

// Удобные функции-конструкторы для построения формул
template <typename T>
typename ExprNode<T>::Ptr Const(T val) {
    return ExprNode<T>::makeConst(val);
}

template <typename T>
typename ExprNode<T>::Ptr Var(const std::string& name) {
    return ExprNode<T>::makeVar(name);
}

template <typename T>
typename ExprNode<T>::Ptr Add(typename ExprNode<T>::Ptr l, typename ExprNode<T>::Ptr r) {
    return ExprNode<T>::makeBinary(BinaryOp::Add, l, r);
}

template <typename T>
typename ExprNode<T>::Ptr Sub(typename ExprNode<T>::Ptr l, typename ExprNode<T>::Ptr r) {
    return ExprNode<T>::makeBinary(BinaryOp::Sub, l, r);
}

template <typename T>
typename ExprNode<T>::Ptr Mul(typename ExprNode<T>::Ptr l, typename ExprNode<T>::Ptr r) {
    return ExprNode<T>::makeBinary(BinaryOp::Mul, l, r);
}

template <typename T>
typename ExprNode<T>::Ptr Div(typename ExprNode<T>::Ptr l, typename ExprNode<T>::Ptr r) {
    return ExprNode<T>::makeBinary(BinaryOp::Div, l, r);
}

// ==========================================
// 2. ПРАВИЛА ОПЕРАЦИЙ (ЛЕГКО РАСШИРЯЕМЫЕ)
// ==========================================

inline std::string opToString(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
    }
    return "?";
}

template <typename T>
T applyBinaryOp(BinaryOp op, T a, T b) {
    switch (op) {
        case BinaryOp::Add: return a + b;
        case BinaryOp::Sub: return a - b;
        case BinaryOp::Mul: return a * b;
        case BinaryOp::Div:
            if (b == T{0}) {
                throw std::runtime_error("Ошибка вычисления: деление на ноль");
            }
            return a / b;
    }
    throw std::logic_error("Неизвестная бинарная операция");
}

// ==========================================
// 3. АЛГОРИТМЫ РАБОТЫ СО СТРУКТУРОЙ ДЕРЕВА
// ==========================================

// Обобщённый алгоритм свёртки дерева (параметрический полиморфизм: правила передаются извне)
template <typename T, typename R, typename ConstFn, typename VarFn, typename BinFn>
R foldExpr(typename ExprNode<T>::Ptr node, ConstFn onConst, VarFn onVar, BinFn onBin) {
    if (!node) {
        throw std::invalid_argument("Пустой узел дерева");
    }
    switch (node->getKind()) {
        case NodeKind::Constant:
            return onConst(node->getValue());
        case NodeKind::Variable:
            return onVar(node->getVarName());
        case NodeKind::Binary: {
            R leftVal = foldExpr<T, R>(node->getLeft(), onConst, onVar, onBin);
            R rightVal = foldExpr<T, R>(node->getRight(), onConst, onVar, onBin);
            return onBin(node->getOp(), leftVal, rightVal);
        }
    }
    throw std::logic_error("Неизвестный тип узла");
}

// 1) Вычисление значения выражения при заданном наборе переменных
template <typename T>
T evaluate(typename ExprNode<T>::Ptr node, const std::map<std::string, T>& vars = {}) {
    return foldExpr<T, T>(
        node,
        [](T val) { return val; },
        [&vars](const std::string& name) -> T {
            auto it = vars.find(name);
            if (it == vars.end()) {
                throw std::runtime_error("Неизвестная переменная: " + name);
            }
            return it->second;
        },
        [](BinaryOp op, T l, T r) {
            return applyBinaryOp<T>(op, l, r);
        }
    );
}

// 2) Получение текстового представления формулы
template <typename T>
std::string toString(typename ExprNode<T>::Ptr node) {
    return foldExpr<T, std::string>(
        node,
        [](T val) {
            std::ostringstream oss;
            oss << val;
            return oss.str();
        },
        [](const std::string& name) { return name; },
        [](BinaryOp op, const std::string& l, const std::string& r) {
            return "(" + l + " " + opToString(op) + " " + r + ")";
        }
    );
}

// 3) Определение глубины дерева
template <typename T>
size_t getDepth(typename ExprNode<T>::Ptr node) {
    if (!node) return 0;
    return foldExpr<T, size_t>(
        node,
        [](T) -> size_t { return 1; },
        [](const std::string&) -> size_t { return 1; },
        [](BinaryOp, size_t l, size_t r) -> size_t { return 1 + std::max(l, r); }
    );
}

// 4) Определение количества узлов
template <typename T>
size_t getNodeCount(typename ExprNode<T>::Ptr node) {
    if (!node) return 0;
    return foldExpr<T, size_t>(
        node,
        [](T) -> size_t { return 1; },
        [](const std::string&) -> size_t { return 1; },
        [](BinaryOp, size_t l, size_t r) -> size_t { return 1 + l + r; }
    );
}

// 5) Получение множества всех используемых переменных
template <typename T>
void collectVariables(typename ExprNode<T>::Ptr node, std::set<std::string>& outVars) {
    if (!node) return;
    if (node->getKind() == NodeKind::Variable) {
        outVars.insert(node->getVarName());
    } else if (node->getKind() == NodeKind::Binary) {
        collectVariables<T>(node->getLeft(), outVars);
        collectVariables<T>(node->getRight(), outVars);
    }
}

template <typename T>
std::set<std::string> getVariables(typename ExprNode<T>::Ptr node) {
    std::set<std::string> vars;
    collectVariables<T>(node, vars);
    return vars;
}

// 6) Упрощение выражения (свёртка константных подвыражений, возвращает новое дерево)
template <typename T>
typename ExprNode<T>::Ptr simplify(typename ExprNode<T>::Ptr node) {
    if (!node) return nullptr;

    // Константы и переменные не изменяются
    if (node->getKind() != NodeKind::Binary) {
        return node;
    }

    auto simpLeft = simplify<T>(node->getLeft());
    auto simpRight = simplify<T>(node->getRight());

    // Если оба подвыражения стали константами — вычисляем их сразу
    if (simpLeft->getKind() == NodeKind::Constant && simpRight->getKind() == NodeKind::Constant) {
        T result = applyBinaryOp<T>(node->getOp(), simpLeft->getValue(), simpRight->getValue());
        return ExprNode<T>::makeConst(result);
    }

    return ExprNode<T>::makeBinary(node->getOp(), simpLeft, simpRight);
}

// ==========================================
// 4. АВТОМАТИЧЕСКИЕ ТЕСТЫ И MAIN
// ==========================================

void runTests() {
    // 1. Защита инвариантов (нельзя создать бинарный узел с пустым операндом)
    bool caughtNull = false;
    try {
        auto bad = Add<double>(Const<double>(1.0), nullptr);
    } catch (const std::invalid_argument&) {
        caughtNull = true;
    }
    assert(caughtNull);

    // 2. Выражение только из констант: (10 + 20) * 3 = 90
    auto constExpr = Mul<int>(Add<int>(Const<int>(10), Const<int>(20)), Const<int>(3));
    assert(evaluate<int>(constExpr) == 90);
    assert(getDepth<int>(constExpr) == 3);
    assert(getNodeCount<int>(constExpr) == 5);
    assert(getVariables<int>(constExpr).empty());

    // 3. Упрощение констант и иммутабельность исходного дерева: (2 + 3) * price -> 5 * price
    auto exprToSimplify = Mul<double>(
        Add<double>(Const<double>(2.0), Const<double>(3.0)),
        Var<double>("price")
    );
    auto simplified = simplify<double>(exprToSimplify);

    // Исходное дерево не изменилось
    assert(getNodeCount<double>(exprToSimplify) == 5);
    assert(toString<double>(exprToSimplify) == "((2 + 3) * price)");

    // Новое дерево упрощено
    assert(getNodeCount<double>(simplified) == 3);
    assert(getDepth<double>(simplified) == 2);
    assert(toString<double>(simplified) == "(5 * price)");
    assert(evaluate<double>(simplified, {{"price", 10.0}}) == 50.0);

    // 4. Обработка неизвестной переменной
    bool caughtUnknownVar = false;
    try {
        evaluate<double>(simplified, {{"wrong_var", 10.0}});
    } catch (const std::runtime_error&) {
        caughtUnknownVar = true;
    }
    assert(caughtUnknownVar);

    // 5. Обработка деления на ноль
    auto divZeroExpr = Div<double>(Var<double>("x"), Sub<double>(Const<double>(5.0), Const<double>(5.0)));
    bool caughtDivZero = false;
    try {
        evaluate<double>(divZeroExpr, {{"x", 100.0}});
    } catch (const std::runtime_error&) {
        caughtDivZero = true;
    }
    assert(caughtDivZero);

    std::cout << "[OK] Все автоматические тесты Задания 2 успешно пройдены!\n\n";
}

template <typename T>
void printExprInfo(const std::string& title, typename ExprNode<T>::Ptr expr) {
    std::cout << "=== " << title << " ===\n";
    std::cout << "Текстовое представление: " << toString<T>(expr) << "\n";
    std::cout << "Глубина дерева: " << getDepth<T>(expr) << "\n";
    std::cout << "Количество узлов: " << getNodeCount<T>(expr) << "\n";

    auto vars = getVariables<T>(expr);
    std::cout << "Используемые переменные: ";
    if (vars.empty()) {
        std::cout << "(нет)";
    } else {
        for (const auto& v : vars) std::cout << v << " ";
    }
    std::cout << "\n\n";
}

int main() {
    runTests();

    // 1. Выражение, состоящее только из констант: (15 - 5) * 4
    auto exprConst = Mul<double>(Sub<double>(Const<double>(15), Const<double>(5)), Const<double>(4));
    printExprInfo<double>("1. Выражение только из констант", exprConst);
    std::cout << "Результат вычисления: " << evaluate<double>(exprConst) << "\n\n";

    // 2. Выражение с одной переменной: (2 + 3) * price
    auto exprOneVar = Mul<double>(Add<double>(Const<double>(2), Const<double>(3)), Var<double>("price"));
    printExprInfo<double>("2. Выражение с одной переменной (до упрощения)", exprOneVar);

    auto exprOneVarSimplified = simplify<double>(exprOneVar);
    printExprInfo<double>("2.1. То же выражение ПОСЛЕ упрощения", exprOneVarSimplified);
    std::cout << "Проверка: исходное дерево осталось прежним: " << toString<double>(exprOneVar) << "\n\n";

    // 3. Выражение с несколькими переменными и уровнями вложенности:
    // (price * count + delivery) / (1 + discount)
    auto exprComplex = Div<double>(
        Add<double>(
            Mul<double>(Var<double>("price"), Var<double>("count")),
            Var<double>("delivery")
        ),
        Add<double>(Const<double>(1.0), Var<double>("discount"))
    );
    printExprInfo<double>("3. Сложное выражение с несколькими переменными", exprComplex);

    // Вычисление одного выражения при различных значениях переменных
    std::map<std::string, double> vars1 = {{"price", 100.0}, {"count", 2.0}, {"delivery", 50.0}, {"discount", 0.25}};
    std::map<std::string, double> vars2 = {{"price", 300.0}, {"count", 1.0}, {"delivery", 0.0},  {"discount", 0.5}};

    std::cout << "Вычисление (набор 1: price=100, count=2, delivery=50, discount=0.25): "
              << evaluate<double>(exprComplex, vars1) << "\n";
    std::cout << "Вычисление (набор 2: price=300, count=1, delivery=0, discount=0.5):   "
              << evaluate<double>(exprComplex, vars2) << "\n\n";

    // 4. Демонстрация перехвата ошибок (неизвестная переменная и деление на ноль)
    std::cout << "=== 4. Демонстрация обработки ошибок ===\n";
    try {
        evaluate<double>(exprComplex, {{"price", 100.0}}); // не хватает переменных
    } catch (const std::exception& e) {
        std::cout << "Перехвачено исключение: " << e.what() << "\n";
    }

    try {
        std::map<std::string, double> varsZero = {{"price", 100.0}, {"count", 2.0}, {"delivery", 50.0}, {"discount", -1.0}};
        evaluate<double>(exprComplex, varsZero); // 1 + (-1) = 0 в знаменателе
    } catch (const std::exception& e) {
        std::cout << "Перехвачено исключение: " << e.what() << "\n";
    }

    return 0;
}