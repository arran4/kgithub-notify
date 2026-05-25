#include "src/utils/FilterParser.h"
#include <QDebug>
#include <QString>

class MyAccessor : public FilterDataAccessor {
public:
    QString getValue(const QString &key) const override {
        if (key == "name") return "kjules";
        if (key == "fork") return "false";
        if (key == "archived") return "false";
        return "";
    }
    QList<QString> getAllValues() const override {
        return {"kjules", "false", "false"};
    }
};

int main() {
    auto ast = FilterParser::parse("fork:false AND archived:false");
    MyAccessor accessor;
    qDebug() << "Evaluating: fork:false AND archived:false";
    qDebug() << "Result:" << ast->evaluate(accessor);
    return 0;
}
