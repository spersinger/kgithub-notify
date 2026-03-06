#include <QUrl>
#include <QUrlQuery>
#include <QString>
#include <iostream>

int main() {
    QString htmlUrl = "https://github.com/user/repo/pull/1";
    QString notificationId = "23162178676";
    QUrl url(htmlUrl);
    QUrlQuery query(url.query());
    query.addQueryItem("notification_referrer_id", notificationId);
    url.setQuery(query);
    std::cout << url.toString().toStdString() << std::endl;
    return 0;
}
