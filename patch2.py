import re

with open('src/NotificationWindow.cpp', 'r') as f:
    data = f.read()

# Replace:
# QString htmlUrl = n.htmlUrl.isEmpty() ? GitHubClient::apiToHtmlUrl(apiUrl, n.id) : n.htmlUrl;
# with:
# QString htmlUrl = GitHubClient::apiToHtmlUrl(apiUrl, n.id);
# (and the same for the others)
data = re.sub(r'QString htmlUrl = n\.htmlUrl\.isEmpty\(\) \? GitHubClient::apiToHtmlUrl\(apiUrl, n\.id\) : n\.htmlUrl;', r'QString htmlUrl = GitHubClient::apiToHtmlUrl(apiUrl, n.id);', data)
data = re.sub(r'QString url = m_notification\.htmlUrl\.isEmpty\(\) \? GitHubClient::apiToHtmlUrl\(m_notification\.url, m_notification\.id\) : m_notification\.htmlUrl;', r'QString url = GitHubClient::apiToHtmlUrl(m_notification.url, m_notification.id);', data)

with open('src/NotificationWindow.cpp', 'w') as f:
    f.write(data)
