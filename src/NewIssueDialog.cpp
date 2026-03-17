#include "NewIssueDialog.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

NewIssueDialog::NewIssueDialog(GitHubClient *client, QWidget *parent)
    : QDialog(parent), m_client(client), m_verifyTimer(new QTimer(this)) {
    setupUI();

    m_verifyTimer->setSingleShot(true);
    connect(m_verifyTimer, &QTimer::timeout, this, &NewIssueDialog::verifyRepo);

    connect(m_client, &GitHubClient::repoVerified, this, &NewIssueDialog::onRepoVerified);
    connect(m_client, &GitHubClient::userReposReceived, this, &NewIssueDialog::onReposReceived);
    connect(m_client, &GitHubClient::errorOccurred, this, &NewIssueDialog::onErrorOccurred);

    loadCache();
}

void NewIssueDialog::setupUI() {
    setWindowTitle(tr("New Issue"));
    resize(400, 150);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *instructionLabel = new QLabel(tr("Select or enter a repository to create an issue in:"), this);
    mainLayout->addWidget(instructionLabel);

    QHBoxLayout *repoLayout = new QHBoxLayout();
    m_repoComboBox = new QComboBox(this);
    m_repoComboBox->setEditable(true);
    m_repoComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_repoComboBox, &QComboBox::currentTextChanged, this, &NewIssueDialog::onRepoTextChanged);
    repoLayout->addWidget(m_repoComboBox);

    m_refreshButton = new QPushButton(tr("Refresh"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &NewIssueDialog::onRefreshClicked);
    repoLayout->addWidget(m_refreshButton);
    mainLayout->addLayout(repoLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    m_createButton = new QPushButton(tr("Create Issue"), this);
    m_createButton->setEnabled(false); // Disabled until verified
    connect(m_createButton, &QPushButton::clicked, this, &NewIssueDialog::onCreateClicked);
    buttonLayout->addWidget(m_createButton);

    mainLayout->addLayout(buttonLayout);
}

void NewIssueDialog::loadCache() {
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QString cachePath = dir.filePath("repos_cache.json");
    QFile file(cachePath);

    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("repos") && obj["repos"].isArray()) {
                m_allRepos = obj["repos"].toArray();

                m_repoComboBox->clear();
                QStringList repoNames;
                for (int i = 0; i < m_allRepos.size(); ++i) {
                    QJsonObject repo = m_allRepos[i].toObject();
                    QString fullName = repo["owner"].toObject()["login"].toString() + "/" + repo["name"].toString();
                    repoNames << fullName;
                }
                repoNames.sort();
                m_repoComboBox->addItems(repoNames);
            }
        }
    }
}

void NewIssueDialog::saveCache() {
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir;
    dir.mkpath(dirPath);

    QString cachePath = dir.filePath("repos_cache.json");
    QFile file(cachePath);

    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject obj;
        // Keep existing last_refresh if any
        QFile existingFile(cachePath);
        if (existingFile.open(QIODevice::ReadOnly)) {
            QJsonDocument existingDoc = QJsonDocument::fromJson(existingFile.readAll());
            if (existingDoc.isObject() && existingDoc.object().contains("last_refresh")) {
                obj["last_refresh"] = existingDoc.object()["last_refresh"];
            }
        }

        obj["repos"] = m_allRepos;

        QJsonDocument doc(obj);
        file.write(doc.toJson());
        file.close();
    }
}

void NewIssueDialog::onRepoTextChanged(const QString &text) {
    m_createButton->setEnabled(false);

    // Basic format check
    if (text.isEmpty() || !text.contains("/")) {
        m_statusLabel->setText("");
        return;
    }

    m_statusLabel->setText(tr("Verifying repository..."));
    m_statusLabel->setStyleSheet("color: gray;");

    m_currentVerifyRepo = text.trimmed();
    m_verifyTimer->start(1000); // 1 second debounce
}

void NewIssueDialog::verifyRepo() {
    if (m_currentVerifyRepo.isEmpty()) return;

    // First check if it's in our cache
    for (int i = 0; i < m_allRepos.size(); ++i) {
        QJsonObject repo = m_allRepos[i].toObject();
        QString fullName = repo["owner"].toObject()["login"].toString() + "/" + repo["name"].toString();
        if (fullName.compare(m_currentVerifyRepo, Qt::CaseInsensitive) == 0) {
            m_statusLabel->setText(tr("Repository found in cache."));
            m_statusLabel->setStyleSheet("color: green;");
            m_createButton->setEnabled(true);
            return;
        }
    }

    // Not in cache, verify via API
    m_client->verifyRepo(m_currentVerifyRepo);
}

void NewIssueDialog::onRepoVerified(const QString &repoFullName, bool exists) {
    if (repoFullName.compare(m_currentVerifyRepo, Qt::CaseInsensitive) != 0) return;

    if (exists) {
        m_statusLabel->setText(tr("Repository verified successfully."));
        m_statusLabel->setStyleSheet("color: green;");
        m_createButton->setEnabled(true);
    } else {
        m_statusLabel->setText(tr("Repository not found or not accessible."));
        m_statusLabel->setStyleSheet("color: red;");
        m_createButton->setEnabled(false);
    }
}

void NewIssueDialog::onCreateClicked() {
    QString repoName = m_repoComboBox->currentText().trimmed();
    if (repoName.isEmpty()) return;

    QUrl url(QString("https://github.com/%1/issues/new").arg(repoName));
    QDesktopServices::openUrl(url);
    accept();
}

void NewIssueDialog::onRefreshClicked() {
    m_refreshButton->setEnabled(false);
    m_statusLabel->setText(tr("Refreshing cache..."));
    m_statusLabel->setStyleSheet("color: gray;");
    m_allRepos = QJsonArray();
    m_client->fetchUserRepos();
}

void NewIssueDialog::onReposReceived(const QJsonArray &repos, const QString &nextPageUrl) {
    for (int i = 0; i < repos.size(); ++i) {
        m_allRepos.append(repos[i]);
    }

    if (!nextPageUrl.isEmpty()) {
        m_client->fetchUserRepos(nextPageUrl);
    } else {
        saveCache();
        loadCache();

        m_refreshButton->setEnabled(true);
        m_statusLabel->setText(tr("Cache refreshed."));
        m_statusLabel->setStyleSheet("color: green;");

        // Re-verify current text if any
        if (!m_repoComboBox->currentText().isEmpty()) {
            onRepoTextChanged(m_repoComboBox->currentText());
        }
    }
}

void NewIssueDialog::onErrorOccurred(const QString &error) {
    m_refreshButton->setEnabled(true);
    m_statusLabel->setText(tr("Error: %1").arg(error));
    m_statusLabel->setStyleSheet("color: red;");
}
