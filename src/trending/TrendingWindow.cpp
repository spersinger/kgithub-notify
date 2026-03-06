#include "TrendingWindow.h"
#include "../GitHubClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QIcon>
#include <QStyle>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QTableWidget>

TrendingWindow::TrendingWindow(GitHubClient *client, QWidget *parent)
    : QWidget(parent, Qt::Window), m_client(client) {
    setWindowTitle(tr("Trending Repos & Devs"));
    resize(800, 600); // make it a bit larger to fit columns

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *topLayout = new QHBoxLayout();

    modeComboBox = new QComboBox(this);
    modeComboBox->addItem(tr("Repositories"));
    modeComboBox->addItem(tr("Developers"));

    timeframeComboBox = new QComboBox(this);
    timeframeComboBox->addItem(tr("Today"));
    timeframeComboBox->addItem(tr("This Week"));
    timeframeComboBox->addItem(tr("This Month"));

    refreshButton = new QPushButton(tr("Refresh"), this);

    topLayout->addWidget(new QLabel(tr("Mode:")));
    topLayout->addWidget(modeComboBox);
    topLayout->addWidget(new QLabel(tr("Timeframe:")));
    topLayout->addWidget(timeframeComboBox);
    topLayout->addStretch();
    topLayout->addWidget(refreshButton);

    tableWidget = new QTableWidget(this);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget->setWordWrap(true);
    tableWidget->verticalHeader()->hide();
    tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(tableWidget);

    connect(refreshButton, &QPushButton::clicked, this, &TrendingWindow::onRefreshClicked);
    connect(modeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrendingWindow::onModeChanged);
    connect(timeframeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TrendingWindow::onRefreshClicked);
    connect(tableWidget, &QTableWidget::itemActivated, this, &TrendingWindow::onItemActivated);
    connect(tableWidget, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTableWidgetItem *item = tableWidget->itemAt(pos);
        if (!item) return;

        QMenu menu(this);
        QAction *openAction = menu.addAction(tr("Open in Browser"));
        QAction *copyAction = menu.addAction(tr("Copy Link"));

        QAction *selectedAction = menu.exec(tableWidget->mapToGlobal(pos));
        if (selectedAction == openAction) {
            onItemActivated(item);
        } else if (selectedAction == copyAction) {
            QString url = item->data(Qt::UserRole).toString();
            QApplication::clipboard()->setText(url);
        }
    });

    if (m_client) {
        connect(m_client, &GitHubClient::rawDataReceived, this, &TrendingWindow::onRawDataReceived);
    }

    // Initial fetch
    onRefreshClicked();
}

void TrendingWindow::onModeChanged(int) {
    onRefreshClicked();
}

void TrendingWindow::onRefreshClicked() {
    if (!m_client) return;

    tableWidget->clear();
    tableWidget->setRowCount(1);
    tableWidget->setColumnCount(1);
    tableWidget->setItem(0, 0, new QTableWidgetItem(tr("Loading...")));

    int daysToSubtract = 1;
    switch (timeframeComboBox->currentIndex()) {
        case 0: daysToSubtract = 1; break;  // Today
        case 1: daysToSubtract = 7; break;  // This Week
        case 2: daysToSubtract = 30; break; // This Month
    }

    QDateTime date = QDateTime::currentDateTime().addDays(-daysToSubtract);
    QString dateStr = date.toString("yyyy-MM-dd");

    QString endpoint;
    if (modeComboBox->currentIndex() == 0) {
        // Repositories
        endpoint = QString("/search/repositories?q=created:>%1&sort=stars&order=desc").arg(dateStr);
    } else {
        // Developers
        endpoint = QString("/search/users?q=created:>%1&sort=followers&order=desc").arg(dateStr);
    }

    lastRequestedUrl = endpoint;
    m_client->requestRaw(endpoint);
}

void TrendingWindow::onRawDataReceived(const QByteArray &data) {
    // Only process if it looks like a search response. We use lastRequestedUrl to match loosely if possible.
    // In our client, requestRaw doesn't pass back the endpoint it requested.
    // So we'll try to parse and check if it's the right format.

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return; // Ignore invalid JSON (might be for something else)
    }

    QJsonObject root = doc.object();
    if (!root.contains("items")) {
        return; // Not a search result
    }

    // Clear the loading text
    tableWidget->clear();
    tableWidget->setRowCount(0);

    QJsonArray items = root["items"].toArray();

    if (items.isEmpty()) {
        tableWidget->setColumnCount(1);
        tableWidget->insertRow(0);
        tableWidget->setItem(0, 0, new QTableWidgetItem(tr("No results found.")));
        return;
    }

    if (modeComboBox->currentIndex() == 0) {
        // Repositories
        tableWidget->setColumnCount(5);
        tableWidget->setHorizontalHeaderLabels({tr("Name"), tr("Stars"), tr("Language"), tr("Description"), tr("URL")});
        tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch); // Description
    } else {
        // Developers
        tableWidget->setColumnCount(2);
        tableWidget->setHorizontalHeaderLabels({tr("Developer"), tr("URL")});
        tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); // URL
    }

    for (int i = 0; i < items.size(); ++i) {
        QJsonObject itemObj = items[i].toObject();
        QString htmlUrl = itemObj["html_url"].toString();

        tableWidget->insertRow(i);

        if (modeComboBox->currentIndex() == 0) {
            // Repositories
            QString name = itemObj["full_name"].toString();
            QString desc = itemObj["description"].toString();
            QString stars = QString::number(itemObj["stargazers_count"].toInt());
            QString lang = itemObj["language"].toString();

            QTableWidgetItem *nameItem = new QTableWidgetItem(name);
            nameItem->setData(Qt::UserRole, htmlUrl);
            QTableWidgetItem *starsItem = new QTableWidgetItem(stars);
            starsItem->setData(Qt::UserRole, htmlUrl);
            QTableWidgetItem *langItem = new QTableWidgetItem(lang);
            langItem->setData(Qt::UserRole, htmlUrl);
            QTableWidgetItem *descItem = new QTableWidgetItem(desc);
            descItem->setData(Qt::UserRole, htmlUrl);
            QTableWidgetItem *urlItem = new QTableWidgetItem(htmlUrl);
            urlItem->setData(Qt::UserRole, htmlUrl);

            tableWidget->setItem(i, 0, nameItem);
            tableWidget->setItem(i, 1, starsItem);
            tableWidget->setItem(i, 2, langItem);
            tableWidget->setItem(i, 3, descItem);
            tableWidget->setItem(i, 4, urlItem);
        } else {
            // Developers
            QString login = itemObj["login"].toString();

            QTableWidgetItem *loginItem = new QTableWidgetItem(login);
            loginItem->setData(Qt::UserRole, htmlUrl);
            QTableWidgetItem *urlItem = new QTableWidgetItem(htmlUrl);
            urlItem->setData(Qt::UserRole, htmlUrl);

            tableWidget->setItem(i, 0, loginItem);
            tableWidget->setItem(i, 1, urlItem);
        }
    }

    tableWidget->resizeRowsToContents();
}

void TrendingWindow::onItemActivated(QTableWidgetItem *item) {
    if (!item) return;
    QString url = item->data(Qt::UserRole).toString();
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}
