#include <iostream>
#include <string>
#include <memory>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <cassert>
#include <algorithm>

using namespace std;

enum NodeType {
    NODE_CONST,
    NODE_VAR,
    NODE_BINARY
};

enum OpType {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV
};

template <typename T>
class ExprNode {
private:
    NodeType type;
    T value;
    string varName;
    OpType op;
    shared_ptr<const ExprNode<T>> left;
    shared_ptr<const ExprNode<T>> right;

public:
    ExprNode(T val) {
        type = NODE_CONST;
        value = val;
        op = OP_ADD;
    }

    ExprNode(string name) {
        if (name.empty()) {
            throw invalid_argument("Empty variable name");
        }
        type = NODE_VAR;
        value = T();
        varName = name;
        op = OP_ADD;
    }

    ExprNode(OpType operation, shared_ptr<const ExprNode<T>> l, shared_ptr<const ExprNode<T>> r) {
        if (l == nullptr || r == nullptr) {
            throw invalid_argument("Binary operation requires two non-null operands");
        }
        type = NODE_BINARY;
        value = T();
        op = operation;
        left = l;
        right = r;
    }

    NodeType getType() const { return type; }
    T getValue() const { return value; }
    string getVarName() const { return varName; }
    OpType getOp() const { return op; }
    shared_ptr<const ExprNode<T>> getLeft() const { return left; }
    shared_ptr<const ExprNode<T>> getRight() const { return right; }
};

template <typename T>
shared_ptr<const ExprNode<T>> makeConst(T val) {
    return make_shared<const ExprNode<T>>(val);
}

template <typename T>
shared_ptr<const ExprNode<T>> makeVar(string name) {
    return make_shared<const ExprNode<T>>(name);
}

template <typename T>
shared_ptr<const ExprNode<T>> makeOp(OpType op, shared_ptr<const ExprNode<T>> l, shared_ptr<const ExprNode<T>> r) {
    return make_shared<const ExprNode<T>>(op, l, r);
}

template <typename T>
T calcOp(OpType op, T a, T b) {
    if (op == OP_ADD) return a + b;
    if (op == OP_SUB) return a - b;
    if (op == OP_MUL) return a * b;
    if (op == OP_DIV) {
        if (b == T(0)) {
            throw runtime_error("Division by zero");
        }
        return a / b;
    }
    throw logic_error("Unknown operation");
}

string opSymbol(OpType op) {
    if (op == OP_ADD) return "+";
    if (op == OP_SUB) return "-";
    if (op == OP_MUL) return "*";
    if (op == OP_DIV) return "/";
    return "?";
}

template <typename T>
T evaluate(shared_ptr<const ExprNode<T>> node, const map<string, T>& vars) {
    if (node->getType() == NODE_CONST) {
        return node->getValue();
    }
    if (node->getType() == NODE_VAR) {
        string name = node->getVarName();
        if (vars.find(name) == vars.end()) {
            throw runtime_error("Unknown variable: " + name);
        }
        return vars.at(name);
    }

    T leftVal = evaluate(node->getLeft(), vars);
    T rightVal = evaluate(node->getRight(), vars);
    return calcOp(node->getOp(), leftVal, rightVal);
}

template <typename T>
string toString(shared_ptr<const ExprNode<T>> node) {
    if (node->getType() == NODE_CONST) {
        ostringstream ss;
        ss << node->getValue();
        return ss.str();
    }
    if (node->getType() == NODE_VAR) {
        return node->getVarName();
    }

    string l = toString(node->getLeft());
    string r = toString(node->getRight());
    return "(" + l + " " + opSymbol(node->getOp()) + " " + r + ")";
}

template <typename T>
int getDepth(shared_ptr<const ExprNode<T>> node) {
    if (node == nullptr) return 0;
    if (node->getType() != NODE_BINARY) {
        return 1;
    }
    int l = getDepth(node->getLeft());
    int r = getDepth(node->getRight());
    return 1 + max(l, r);
}

template <typename T>
int getNodeCount(shared_ptr<const ExprNode<T>> node) {
    if (node == nullptr) return 0;
    if (node->getType() != NODE_BINARY) {
        return 1;
    }
    return 1 + getNodeCount(node->getLeft()) + getNodeCount(node->getRight());
}

template <typename T>
void collectVars(shared_ptr<const ExprNode<T>> node, set<string>& result) {
    if (node == nullptr) return;
    if (node->getType() == NODE_VAR) {
        result.insert(node->getVarName());
    } else if (node->getType() == NODE_BINARY) {
        collectVars(node->getLeft(), result);
        collectVars(node->getRight(), result);
    }
}

template <typename T>
set<string> getVariables(shared_ptr<const ExprNode<T>> node) {
    set<string> res;
    collectVars(node, res);
    return res;
}

template <typename T>
shared_ptr<const ExprNode<T>> simplify(shared_ptr<const ExprNode<T>> node) {
    if (node->getType() != NODE_BINARY) {
        return node;
    }

    auto leftSimp = simplify(node->getLeft());
    auto rightSimp = simplify(node->getRight());

    if (leftSimp->getType() == NODE_CONST && rightSimp->getType() == NODE_CONST) {
        T res = calcOp(node->getOp(), leftSimp->getValue(), rightSimp->getValue());
        return makeConst<T>(res);
    }

    return makeOp<T>(node->getOp(), leftSimp, rightSimp);
}

void runTests() {
    bool errorCaught = false;
    try {
        auto bad = makeOp<double>(OP_ADD, makeConst<double>(1.0), nullptr);
    } catch (const invalid_argument&) {
        errorCaught = true;
    }
    assert(errorCaught == true);

    auto e1 = makeOp<int>(OP_MUL, makeOp<int>(OP_ADD, makeConst<int>(10), makeConst<int>(20)), makeConst<int>(3));
    map<string, int> emptyVars;
    assert(evaluate(e1, emptyVars) == 90);
    assert(getDepth(e1) == 3);
    assert(getNodeCount(e1) == 5);

    auto e2 = makeOp<double>(OP_MUL, makeOp<double>(OP_ADD, makeConst<double>(2), makeConst<double>(3)), makeVar<double>("price"));
    auto simp = simplify(e2);

    assert(getNodeCount(e2) == 5);
    assert(getNodeCount(simp) == 3);
    assert(toString(simp) == "(5 * price)");

    bool varError = false;
    try {
        map<string, double> v;
        evaluate(simp, v);
    } catch (const runtime_error&) {
        varError = true;
    }
    assert(varError == true);

    bool divZeroError = false;
    try {
        auto divZero = makeOp<double>(OP_DIV, makeConst<double>(10), makeConst<double>(0));
        map<string, double> v;
        evaluate(divZero, v);
    } catch (const runtime_error&) {
        divZeroError = true;
    }
    assert(divZeroError == true);

    cout << "Тесты Задания 2 пройдены успешно!\n\n";
}

int main() {
    runTests();

    auto exprConst = makeOp<double>(OP_MUL, makeOp<double>(OP_SUB, makeConst<double>(15), makeConst<double>(5)), makeConst<double>(4));
    map<string, double> noVars;
    cout << "1. Константное выражение: " << toString(exprConst) << " = " << evaluate(exprConst, noVars) << "\n";
    cout << "   Глубина: " << getDepth(exprConst) << ", Узлов: " << getNodeCount(exprConst) << "\n\n";

    auto exprOne = makeOp<double>(OP_MUL, makeOp<double>(OP_ADD, makeConst<double>(2), makeConst<double>(3)), makeVar<double>("price"));
    auto exprSimp = simplify(exprOne);
    cout << "2. До упрощения:    " << toString(exprOne) << " (узлов: " << getNodeCount(exprOne) << ")\n";
    cout << "   После упрощения: " << toString(exprSimp) << " (узлов: " << getNodeCount(exprSimp) << ")\n\n";

    auto exprComplex = makeOp<double>(
        OP_DIV,
        makeOp<double>(OP_ADD, makeOp<double>(OP_MUL, makeVar<double>("price"), makeVar<double>("count")), makeVar<double>("delivery")),
        makeOp<double>(OP_ADD, makeConst<double>(1), makeVar<double>("discount"))
    );

    cout << "3. Сложное выражение: " << toString(exprComplex) << "\n";
    cout << "   Глубина: " << getDepth(exprComplex) << ", Узлов: " << getNodeCount(exprComplex) << "\n";
    cout << "   Переменные: ";
    for (const string& v : getVariables(exprComplex)) {
        cout << v << " ";
    }
    cout << "\n";

    map<string, double> vars1 = {{"price", 100}, {"count", 2}, {"delivery", 50}, {"discount", 0.25}};
    map<string, double> vars2 = {{"price", 300}, {"count", 1}, {"delivery", 0}, {"discount", 0.5}};
    cout << "   Значение при наборе 1: " << evaluate(exprComplex, vars1) << "\n";
    cout << "   Значение при наборе 2: " << evaluate(exprComplex, vars2) << "\n\n";

    cout << "4. Проверка ошибок:\n";
    try {
        map<string, double> badVars = {{"price", 100}};
        evaluate(exprComplex, badVars);
    } catch (const exception& e) {
        cout << "   Ошибка: " << e.what() << "\n";
    }

    try {
        map<string, double> zeroVars = {{"price", 100}, {"count", 2}, {"delivery", 50}, {"discount", -1}};
        evaluate(exprComplex, zeroVars);
    } catch (const exception& e) {
        cout << "   Ошибка: " << e.what() << "\n";
    }

    return 0;
}