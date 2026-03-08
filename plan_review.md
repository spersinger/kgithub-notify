The user said "the 'star' column doesn't seem to have been implmented it's supposed to show if I have starred the repo."
Why wouldn't it be implemented? Let me look at the code again:
```cpp
// Fetch starred status
QUrl url("https://api.github.com/user/starred/" + name);
QNetworkRequest request = m_client->createAuthenticatedRequest(url);
QNetworkReply *reply = m_netManager->get(request);
reply->setProperty("fullName", name);
```
Wait, the user is saying "doesn't seem to have been implemented" maybe because there is NO visual column?
Wait! In `tableWidget->setHorizontalHeaderLabels({tr("★"), tr("Name"), tr("Stars"), ...});` I did add it.
If they pulled my changes, they would see the "★" column.
Did they actually test the code? Yes, they said "The 'today' filter isn't working any more either".
Why did the "today" filter stop working?
Because my `onRawDataReceived` changes might have broken the parsing?
No, I only changed `setColumnCount(6)`. I didn't touch the request string.
Let's see: `endpoint = QString("/search/repositories?q=created:>%1%2&sort=stars&order=desc").arg(dateStr).arg(langQuery);`
If `langQuery` is empty, it works.

Is it possible that the API rate-limits the 30 `GET /user/starred/...` requests immediately, so they all return 403 Forbidden?
Yes! GitHub API strongly discourages making concurrent requests. 30 concurrent `GET` requests from a desktop app will trigger the secondary rate limit (abuse mechanism) and return 403.
When it returns 403, `reply->error()` is `QNetworkReply::ContentAccessDenied` and the status code is 403. So it won't be 204. Thus, no star is shown!
That's why "the star column doesn't seem to have been implemented"! It's there, but it's empty because all requests get 403'd.
To fix this, we should NOT make 30 concurrent requests.
How can we check if the user starred these 30 repos?
Option A: Fetch the user's starred repos `GET /user/starred` and paginate. If a user has 10,000 starred repos, fetching them all will take time. But we can cache it in `GitHubClient` or `TrendingWindow` and update it periodically.
Option B: Make the requests sequentially. Create a queue of `name`s. When one `QNetworkReply` finishes, start the next one. This will take ~30 * 100ms = 3 seconds to populate the stars. That's acceptable and won't trigger the abuse limit as easily (though 1 per second is safer for long bursts, 30 sequential might be okay).
Actually, GitHub's secondary rate limit says: "Do not make requests concurrently." Sequential is fine.

Let's do Option B: Sequential queue for checking starred status.
```cpp
private:
    QStringList m_starredCheckQueue;
    void processNextStarredCheck();
```
When `onRawDataReceived` parses the 30 repos, it clears `m_starredCheckQueue` and pushes the 30 names into it.
Then it calls `processNextStarredCheck()`.
```cpp
void TrendingWindow::processNextStarredCheck() {
    if (m_starredCheckQueue.isEmpty()) return;
    QString name = m_starredCheckQueue.takeFirst();
    QUrl url("https://api.github.com/user/starred/" + name); // Or use m_client->apiUrl() if we had it. Let's ask m_client to do it if possible, but m_netManager is fine.
    QNetworkRequest request = m_client->createAuthenticatedRequest(url);
    QNetworkReply *reply = m_netManager->get(request);
    reply->setProperty("fullName", name);
}

void TrendingWindow::onRepoStarredCheckFinished(QNetworkReply *reply) {
    // ... handle reply ...
    reply->deleteLater();
    // then:
    processNextStarredCheck();
}
```
This is MUCH safer and won't trigger 403s.

For the "today" filter:
Wait, `QDateTime::currentDateTime().addDays(-1)` gives yesterday's date.
If today is `2024-05-19`, `created:>2024-05-18`. If it's early in the day (e.g. 01:00 AM), `created:>2024-05-18` might not have many repos with high stars yet, because it only covers repos created after midnight yesterday.
Actually, if it's 2024-05-19 01:00 AM, `addDays(-1)` is 2024-05-18 01:00 AM. `toString("yyyy-MM-dd")` is `2024-05-18`.
`created:>2024-05-18` means created on `2024-05-19` OR LATER. So it ONLY includes repos created today!
If they created the repo today, it probably has 0 stars.
Wait, GitHub's search syntax: `created:>2024-05-18` means strictly greater than 2024-05-18, i.e., starting from `2024-05-19 00:00:00`.
If the user runs this at 08:00 AM, there are very few repos created today with many stars.
If they want "Today" trending, they probably want `created:>=yesterday` or similar?
Let's change it to `created:>=` `addDays(-1)` ?
Actually, `created:>=` is supported. `created:>=%1`. So `created:>=2024-05-18` includes all of yesterday and today. That's a 48-hour window at most, which is better for "Today" trending (since repos take time to get stars).
I'll change it to `created:>=%1`.

Let's make a clear plan for the fixes.
