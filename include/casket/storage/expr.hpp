#pragma once
#include <casket/storage/field.hpp>

/** \file expr.hpp
    Contains Expr-class hierarchy and operator overloadings for them.*/
namespace casket::storage {

/** A base class for expression in WHERE - clause.
    See \ref usage_selecting_persistents */
class Expr {
public:
    /// constant for True expression
    static const char* True;
    // default expression is true
    virtual std::string asString() const {
        return True;
    }

    const std::vector<std::string>& getExtraTables() const {
        return extraTables;
    }

    virtual ~Expr() {
    }

protected:
    // extra tables to be joined
    std::vector<std::string> extraTables;
};

/** used to inject custom expression into WHERE-clause */
class RawExpr : public Expr {

public:
    RawExpr(const std::string& e)
        : expr(e) {
    }
    virtual std::string asString() const {
        return expr;
    }

private:
    std::string expr;
};
/** used to connect two expressions */
class Connective : public Expr {
private:
    std::string op;

protected:
    const Expr &e1, &e2;

    Connective(const std::string& o, const Expr& e1_, const Expr& e2_)
        : op(o)
        , e1(e1_)
        , e2(e2_) {
    }

public:
    virtual ~Connective() {
    }

    virtual std::string asString() const {
        std::string res = "(" + e1.asString() + ") " + op + " (" + e2.asString() + ")";
        return res;
    }
};
/** connects two expressions with and-operator. */
class And : public Connective {
public:
    And(const Expr& e1_, const Expr& e2_)
        : Connective("and", e1_, e2_) {
    }
    virtual std::string asString() const {
        if (e1.asString() == True)
            return e2.asString();
        else if (e2.asString() == True)
            return e1.asString();
        else
            return Connective::asString();
    }
};
/** connects two expression with or-operator. */
class Or : public Connective {
public:
    Or(const Expr& e1_, const Expr& e2_)
        : Connective("or", e1_, e2_) {
    }
    virtual std::string asString() const {
        if ((e1.asString() == True) || (e2.asString() == True))
            return True;
        else
            return Connective::asString();
    }
};
/** negates expression */
class Not : public Expr {
private:
    const Expr& exp;

public:
    Not(const Expr& _exp)
        : exp(_exp) {
    }
    virtual std::string asString() const {
        return "not (" + exp.asString() + ")";
    }
};
/** base class of operators in expressions */
class Oper : public Expr {
protected:
    const FieldType& field;
    std::string op;
    std::string data;
    bool escape;

    Oper(const FieldType& fld, const std::string& o, const std::string& d)
        : field(fld)
        , op(o)
        , data(d)
        , escape(true) {
        extraTables.push_back(fld.table());
    }
    Oper(const FieldType& fld, const std::string& o, const FieldType& f2)
        : field(fld)
        , op(o)
        , data(f2.fullName())
        , escape(false) {
        extraTables.push_back(fld.table());
    }

public:
    virtual std::string asString() const {
        std::string res;
        res += field.fullName() + " " + op + " " + (escape ? escapeSQL(data) : data);
        return res;
    }
};
/** equality operator */
class Eq : public Oper {
public:
    Eq(const FieldType& fld, const std::string& d)
        : Oper(fld, "=", d) {
    }
    Eq(const FieldType& fld, const FieldType& f2)
        : Oper(fld, "=", f2) {
    }
};
/** inequality operator */
class NotEq : public Oper {
public:
    NotEq(const FieldType& fld, const std::string& d)
        : Oper(fld, "<>", d) {
    }
    NotEq(const FieldType& fld, const FieldType& f2)
        : Oper(fld, "<>", f2) {
    }
};
/** greater than operator */
class Gt : public Oper {
public:
    Gt(const FieldType& fld, const std::string& d)
        : Oper(fld, ">", d) {
    }
    Gt(const FieldType& fld, const FieldType& d)
        : Oper(fld, ">", d) {
    }
};
/** greater or equal operator */
class GtEq : public Oper {
public:
    GtEq(const FieldType& fld, const std::string& d)
        : Oper(fld, ">=", d) {
    }
    GtEq(const FieldType& fld, const FieldType& d)
        : Oper(fld, ">=", d) {
    }
};
/** less than operator */
class Lt : public Oper {
public:
    Lt(const FieldType& fld, const std::string& d)
        : Oper(fld, "<", d) {
    }
    Lt(const FieldType& fld, const FieldType& d)
        : Oper(fld, "<", d) {
    }
};
/** less than or equal operator */
class LtEq : public Oper {
public:
    LtEq(const FieldType& fld, const std::string& d)
        : Oper(fld, "<=", d) {
    }
    LtEq(const FieldType& fld, const FieldType& d)
        : Oper(fld, "<=", d) {
    }
};
/** like operator */
class Like : public Oper {
public:
    Like(const FieldType& fld, const std::string& d)
        : Oper(fld, "like", d) {
    }
};

/** in operator */
class In : public Oper {
public:
    In(const FieldType& fld, const std::string& set)
        : Oper(fld, "in", "(" + set + ")"){};
    In(const FieldType& fld, const QuerySelect& s);
    virtual std::string asString() const {
        return field.fullName() + " " + op + " " + data;
    }
};
And operator&&(const Expr& o1, const Expr& o2);
Or operator||(const Expr& o1, const Expr& o2);
template <class T>
Eq operator==(const FieldType& fld, const T& o2) {
    return Eq(fld, toString(o2));
}
Eq operator==(const FieldType& fld, const FieldType& f2);
Gt operator>(const FieldType& fld, const FieldType& o2);
GtEq operator>=(const FieldType& fld, const FieldType& o2);
Lt operator<(const FieldType& fld, const FieldType& o2);
LtEq operator<=(const FieldType& fld, const FieldType& o2);
NotEq operator!=(const FieldType& fld, const FieldType& f2);

template <class T>
Gt operator>(const FieldType& fld, const T& o2) {
    return Gt(fld, toString(o2));
}

template <class T>
GtEq operator>=(const FieldType& fld, const T& o2) {
    return GtEq(fld, toString(o2));
}

template <class T>
Lt operator<(const FieldType& fld, const T& o2) {
    return Lt(fld, toString(o2));
}

template <class T>
LtEq operator<=(const FieldType& fld, const T& o2) {
    return LtEq(fld, toString(o2));
}
template <class T>
NotEq operator!=(const FieldType& fld, const T& o2) {
    return NotEq(fld, toString(o2));
}

Not operator!(const Expr& exp);
} // namespace casket::storage
